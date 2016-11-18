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
	Speed	= 10;
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
		_Current[i]	= 0;
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
		pwm->SetFrequency(500);
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
static const byte Levels[]	= { 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,3,3,3,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,6,6,6,6,6,6,6,6,7,7,7,7,7,7,8,8,8,8,8,8,9,9,9,9,9,10,10,10,10,10,11,11,11,11,12,12,12,12,13,13,13,14,14,14,15,15,15,15,16,16,17,17,17,18,18,18,19,19,20,20,21,21,21,22,22,23,23,24,24,25,26,26,27,27,28,28,29,30,30,31,32,32,33,34,35,35,36,37,38,39,39,40,41,42,43,44,45,46,47,48,49,50,51,52,54,55,56,57,58,60,61,62,64,65,67,68,69,71,73,74,76,77,79,81,83,85,86,88,90,92,94,96,98,101,103,105,107,110,112,115,117,120,122,125,128,131,133,136,139,142,146,149,152,155,159,162,166,170,173,177,181,185,189,193,197,202,206,211,215,220,225,230,235,240,245,251,255 };

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
			byte x	= _Current[i];

			if(x < _Pulse[i])
				x++;
			else if(x > _Pulse[i])
				x--;
			else
				end++;

			// 向前跳一级
			byte cur	= Levels[x];
			pwm.SetDuty(i, cur);

			//debug_printf("Dimmer::Flush %d Pulse=%d => %d x=%d\r\n", i+1, cur, _Pulse[i], x);

			_Current[i]	= x;
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

		//byte cur	= pwm.GetDuty(i);
		//byte x		= GetX(cur);

		//_Current[i]	= x;
		byte x	= _Current[i];
		_Pulse[i]	= vs[i];

		if(cfg.SaveLast) cfg.Values[i]	= vs[i];

		debug_printf("Dimmer::Set %d Pulse=%d => %d x=%d\r\n", i+1, Levels[x], vs[i], x);
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
