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
	Min		= 0;
	Max		= 255;
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

// 调光亮度等级公式：2^(x/32) * 256 - 1
static const ushort Levels[]	= { 261,266,272,278,284,291,297,303,310,317,324,331,338,346,353,361,369,377,385,394,402,411,420,430,439,449,458,469,479,489,500,511,522,534,545,557,570,582,595,608,621,635,649,663,678,692,708,723,739,755,772,789,806,824,842,860,879,898,918,938,959,980,1001,1023,1045,1068,1092,1116,1140,1165,1191,1217,1243,1271,1299,1327,1356,1386,1416,1447,1479,1511,1544,1578,1613,1648,1684,1721,1759,1797,1837,1877,1918,1960,2003,2047,2092,2138,2185,2232,2281,2331,2382,2434,2488,2542,2598,2655,2713,2773,2833,2895,2959,3024,3090,3157,3227,3297,3370,3443,3519,3596,3675,3755,3837,3921,4007,4095,4185,4276,4370,4466,4564,4663,4766,4870,4977,5086,5197,5311,5427,5546,5667,5792,5918,6048,6181,6316,6454,6596,6740,6888,7038,7193,7350,7511,7676,7844,8015,8191,8370,8554,8741,8932,9128,9328,9532,9741,9954,10172,10395,10623,10855,11093,11336,11584,11838,12097,12362,12633,12909,13192,13481,13776,14078,14386,14701,15023,15352,15688,16032,16383,16742,17108,17483,17866,18257,18657,19065,19483,19910,20346,20791,21246,21712,22187,22673,23169,23677,24195,24725,25267,25820,26385,26963,27553,28157,28773,29404,30047,30705,31378,32065,32767,33485,34218,34967,35733,36515,37315,38132,38967,39820,40692,41583,42494,43424,44375,45347,46340,47355,48392,49451,50534,51641,52772,53927,55108,56315,57548,58808,60096,61412,62756,64131,65535 };

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
		byte d	= vs[i];

		// 最大最小值
		if(d < Min) d	= Min;
		if(d > Max) d	= Max;

		_Pulse[i]	= d;

		if(cfg.SaveLast) cfg.Values[i]	= d;

#if DEBUG
		byte x	= _Current[i];
		debug_printf("Dimmer::Set %d Pulse=%d => %d x=%d\r\n", i+1, Levels[x], Levels[d], x);
#endif
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
