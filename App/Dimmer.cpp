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
		if(cfg.PowerOn >= 0x10) Animate(cfg.PowerOn, (cfg.Speed << 8) + 100);
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
static const ushort Levels[]	= { 256,262,267,273,279,285,292,298,305,311,318,325,332,340,347,355,363,371,379,387,396,404,413,422,431,441,451,461,471,481,492,502,513,525,536,548,560,572,585,598,611,624,638,652,667,681,696,712,727,743,760,776,793,811,829,847,865,884,904,924,944,965,986,1008,1030,1053,1076,1099,1123,1148,1173,1199,1226,1253,1280,1308,1337,1366,1396,1427,1459,1491,1523,1557,1591,1626,1662,1698,1736,1774,1813,1853,1894,1935,1978,2021,2066,2111,2157,2205,2253,2303,2354,2405,2458,2512,2568,2624,2682,2741,2801,2862,2925,2990,3055,3123,3191,3262,3333,3407,3481,3558,3636,3716,3798,3881,3967,4054,4143,4234,4327,4422,4520,4619,4721,4824,4930,5039,5150,5263,5379,5497,5618,5741,5868,5997,6128,6263,6401,6542,6685,6832,6983,7136,7293,7454,7617,7785,7956,8131,8310,8493,8679,8870,9065,9264,9468,9676,9889,10107,10329,10556,10788,11025,11268,11515,11769,12027,12292,12562,12838,13121,13409,13704,14005,14313,14628,14950,15278,15614,15958,16309,16667,17034,17408,17791,18182,18582,18990,19408,19835,20271,20717,21172,21638,22113,22600,23097,23605,24124,24654,25196,25750,26316,26895,27486,28091,28708,29340,29985,30644,31318,32006,32710,33429,34165,34916,35683,36468,37270,38089,38927,39783,40658,41552,42465,43399,44353,45329,46325,47344,48385,49449,50536,51647,52783,53944,55130,56342,57581,58847,60141,61463,62815,64196,65535 };

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
			auto cur	= Levels[x];
			//pwm.SetDuty(i, cur);
			pwm.Pulse[i]	= ((int)cur * pwm.Period) >> 16;

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

		debug_printf("Dimmer::Set %d Pulse=%d => %d x=%d\r\n", i+1, Levels[x], Levels[vs[i]], x);
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
