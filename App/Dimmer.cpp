#include "Sys.h"

#include "Device/Pwm.h"

#include "Config.h"

#include "Dimmer.h"

/******************************** 调光配置 ********************************/
DimmerConfig::DimmerConfig()
{
	_Name	= "Dimmer";
	_Start	= &Values;
	_End	= &TagEnd;
	Init();

	SaveLast= true;
	PowerOn	= 0x10;	// 默认呼吸灯动感模式
	Speed	= 100;
	for(int i=0; i<4; i++) Values[i]	= 0xFF;
}

static DimmerConfig* _cfg	= nullptr;
static ByteArray	_bak;
static uint			_saveTask	= 0;

static void SaveTask(void* param)
{
	if(!_cfg) return;

	// 比较数据，如果不同则保存
	auto bs	= _cfg->ToArray();
	if(_bak.Length() == 0 || bs != _bak)
	{
		_bak	= bs;
		_cfg->Save();
	}
}

static DimmerConfig* InitConfig()
{
	static DimmerConfig	cfg;
	if(!_cfg)
	{
		_cfg	= &cfg;
		_cfg->Load();

		_bak	= cfg.ToArray();
		if(!_saveTask) _saveTask	= Sys.AddTask(SaveTask, nullptr, 1000, 10000, "保存配置");
	}

	return _cfg;
}

/******************************** 调光Dimmer ********************************/

Dimmer::Dimmer()
{
	_Pwm	= nullptr;
	Opened	= false;
	Config	= nullptr;

	_task	= 0;
	_taskAnimate	= 0;

	for(int i=0; i<4; i++)
	{
		_Pulse[i]	= 0;
		_Next[i]	= 0;
	}
}

Dimmer::~Dimmer()
{
	if(Opened) Close();
}

void Dimmer::Init(TIMER timer, int channels)
{
	// 初始化Pwm，指定定时器、频率、通道
	if(!_Pwm && timer != Timer0)	_Pwm	= new Pwm(timer);

	Init(_Pwm, channels);
}

void Dimmer::Init(Pwm* pwm, int channels)
{
	Config	= InitConfig();

	if(pwm)
	{
		_Pwm	= pwm;
		pwm->SetFrequency(100);
		for(int i=0; i< channels; i++)
		{
			// 打开通道，初始值为0，渐变效果
			pwm->Enabled[i]	= true;
			pwm->Pulse[i]	= 0;
		}
	}
}

void Dimmer::Open()
{
	if(Opened) return;

	// 配置数据备份，用于比较保存
	auto& cfg	= *Config;

	// 先打开Pwm再设置脉宽，否则可能因为周期还没来得及计算

	// 是否上电默认打开
	if(cfg.PowerOn)
	{
		_Pwm->Open();

		// 打开动感模式
		if(cfg.PowerOn >= 0x10) Animate(cfg.PowerOn);
	}

	// 是否恢复上次保存？
	if(cfg.SaveLast)
		Set(cfg.Values);
	else
	{
		// 出厂默认最大亮度100%
		byte vs[]	= { 0xFF, 0xFF, 0xFF, 0xFF };
		Set(vs);
	}

	Opened	= true;
}

void Dimmer::Close()
{
	if(!Opened) return;

	_Pwm->Close();

	Sys.RemoveTask(_task);
	Sys.RemoveTask(_taskAnimate);

	Opened	= false;
}

// 等级关键点。共20点，要是刚好32点就好了
static byte Levels[]	= { 1,1,2,2,3,4,6,8,10,14,19,25,33,44,59,80,107,143,191,255 };
// 根据Y获取X
static byte GetX(byte y)
{
	if(y <= 1) return 0;
	if(y == 255) return 255;

	// 找到第一个大于等于当前值的等级，注意方向
	int k	= 0;
	for(; k<ArrayLength(Levels) && y > Levels[k]; k++);

	// 刚好落在关键点
	if(y == Levels[k]) return k * 256 / ArrayLength(Levels);

	// 当前值和前一个值构成一个区间，要查找的y就在这个区间里面
	byte cur	= Levels[k];
	byte pri	= k==0 ? 0 : Levels[k - 1];

	// 放大到256级。整段以及区间
	// k * 256 / ArrayLength(Levels)
	// (y - pri) / (cur - pri) * 256 / ArrayLength(Levels)
	byte z	= (y - pri) * 256 / ArrayLength(Levels) / (cur - pri);
	if(k > 0) k--;
	byte x	= k * 256 / ArrayLength(Levels);

	return x + z;
}

// 根据X获取Y
static byte GetY(byte x)
{
	if(x == 0) return 1;
	if(x == 255) return 255;

	// 计算整段以及区间
	byte k	= x * ArrayLength(Levels) / 256;

	// 已经到达结尾
	if(k >= ArrayLength(Levels) - 1) return Levels[k];

	byte cur	= Levels[k];
	byte next	= Levels[k + 1];

	byte z	= x - k * 256 / ArrayLength(Levels);
	// z * ArrayLength(Levels) / 256 * (next - cur)
	z	= z * (next - cur) * ArrayLength(Levels) / 256;

	return Levels[k] + z;
}

// 刷新任务。产生渐变效果
void Dimmer::FlushTask()
{
	auto& pwm	= *_Pwm;

	int end	= 0;
	// 每一个通道产生渐变效果
	for(int i=0; i<4; i++)
	{
		if(pwm.Enabled[i])
		{
			byte x		= _Next[i];
			// 向前跳一级
			byte cur	= GetY(x);
			pwm.SetDuty(i, cur);

			if(cur == 255)
				end++;
			else if(cur < _Pulse[i])
				x++;
			else if(cur > _Pulse[i])
				x--;
			else
				end++;

			debug_printf("Dimmer::Flush %d Pulse=%d => %d x=%d\r\n", i+1, cur, _Pulse[i], x);
			_Next[i]	= x;
		}
		else
			end++;
	}
	// 刷新数据
	pwm.Flush();

	// 所有通道都结束，则关闭任务
	if(end == 4)
	{
		debug_printf("所有通道结束 \r\n");
		Sys.SetTask(_task, false);
		return;
	}
}

#define FlushLevel	100
void Dimmer::Set(byte vs[4])
{
	auto& pwm	= *_Pwm;
	auto& cfg	= *Config;
	// 等分计算步长
	for(int i=0; i<4; i++)
	{
		if(!pwm.Enabled[i]) continue;

		// 没有改变则不需要调节
		//if((((vs[i] + 1) * pwm.Period) >> 8) == pwm.Pulse[i]) continue;

		byte cur	= pwm.GetDuty(i);
		byte x		= GetX(cur);

		_Next[i]	= x;
		_Pulse[i]	= vs[i];

		if(cfg.SaveLast) cfg.Values[i]	= vs[i];

		debug_printf("Dimmer::Set %d Pulse=%d => %d x=%d\r\n", i+1, cur, vs[i], x);
	}

	debug_printf("开始调节……\r\n");
	if(cfg.Speed > 0)
	{
		if(!_task) _task	= Sys.AddTask(&Dimmer::FlushTask, this, 10, cfg.Speed, "灯光渐变");
		Sys.SetTask(_task, true, 0);
	}
	else
	{
		for(int i=0; i<4; i++)
			pwm.Pulse[i]	= vs[i];

		// 刷新数据
		pwm.Flush();
	}
}

void Dimmer::Change(bool open)
{
	if(open)
		_Pwm->Open();
	else
		_Pwm->Close();
}

void Dimmer::AnimateTask()
{
	bool on	= _AnimateData[1];
	byte vs1[4];
	byte vs2[4];
	Buffer bs1(vs1, 4);
	Buffer bs2(vs2, 4);
	bs1.Set(0, 0, 4);
	bs2.Set(0, 0, 4);
	switch(_AnimateData[0])
	{
		case 0x10:	// 亮暗呼吸灯
		{
			//bs1.Set(0, 0, 4);
			bs1.Set(0xFF, 0, 4);
			break;
		}
		case 0x11:	// 单通道1
		{
			vs2[0]	= 0xFF;
			break;
		}
		case 0x12:	// 单通道2
		{
			vs2[1]	= 0xFF;
			break;
		}
		case 0x13:	// 单通道3
		{
			vs2[2]	= 0xFF;
			break;
		}
		case 0x14:	// 单通道4
		{
			vs2[3]	= 0xFF;
			break;
		}
		case 0x15:	// 通道交错
		{
			vs1[1]	= vs1[3]	= 0xFF;
			vs2[1]	= vs2[3]	= 0;
			break;
		}
	}

	Set(on ? vs1 : vs2);

	on	= !on;
	_AnimateData[1]	= on;
}

void Dimmer::Animate(byte mode, int ms)
{
	if(mode == 0)
	{
		Sys.RemoveTask(_taskAnimate);
		return;
	}

	debug_printf("Dimmer::Animate 动感模式 0x%02x ms=%d \r\n", mode, ms);

	_AnimateData[0]	= mode;

	if(!_taskAnimate)
		_taskAnimate	= Sys.AddTask(&Dimmer::AnimateTask, this, 100, ms, "动感模式");
	else
		Sys.SetTaskPeriod(_taskAnimate, ms);
}
