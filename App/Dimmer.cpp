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

	SaveLast	= true;
	PowerOn		= true;
	Gradient	= 2000;
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
static byte Levels[]	= {1,1,2,2,3,4,6,8,10,14,19,25,33,44,59,80,107,143,191,255,};

Dimmer::Dimmer()
{
	_Pwm	= nullptr;
	Opened	= false;
	Config	= nullptr;

	_task	= 0;

	for(int i=0; i<4; i++)
	{
		_Pulse[i]	= 0;
		_Step[i]	= 0;
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
	if(cfg.PowerOn) _Pwm->Open();

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

	Opened	= false;
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
			int now	= pwm.Pulse[i];
			int dst	= _Pulse[i];
			int st	= _Step[i];
			if(now + st < dst)	// 递增
				pwm.Pulse[i]	+= st;
			else if(now - st > dst)// 递减
				pwm.Pulse[i]	-= st;
			else if(now != dst)	// 最后一步接近
				pwm.Pulse[i]	= dst;
			else
				end++;
			//debug_printf("Dimmer::Flush %d Pulse=%d => %d\r\n", i+1, pwm.Pulse[i], _Pulse[i]);
		}
		else
			end++;
	}
	// 所有通道都结束，则关闭任务
	if(end == 4)
	{
		debug_printf("所有通道结束 \r\n");
		Sys.SetTask(_task, false);
		return;
	}

	// 刷新数据
	pwm.Flush();
}

#define FlushLevel	100
void Dimmer::Set(byte vs[4])
{
	auto& pwm	= *_Pwm;
	// 等分计算步长
	for(int i=0; i<4; i++)
	{
		if(!pwm.Enabled[i]) continue;

		// 冷暖光一共256级，需要除以256再乘以周期得到占空比宽度
		_Pulse[i]	= ((vs[i] + 1) * pwm.Period) >> 8;

		int st	= ((int)pwm.Pulse[i] - (int)_Pulse[i]) / FlushLevel;
		if(st < 0)
			st	= -st;
		else if(st == 0)
			st	= 1;
		_Step[i]	= st;

		debug_printf("Dimmer::Set %d Value=%d Pulse=%d => %d _Step=%d\r\n", i+1, vs[i], pwm.Pulse[i], _Pulse[i], _Step[i]);
	}

	if (Config->SaveLast)
	{
		for(int i=0; i<4; i++)
			Config->Values[i]	= vs[i];
	}

	if(Config->Gradient > 0)
	{
		if(!_task) _task	= Sys.AddTask(&Dimmer::FlushTask, this, 10, Config->Gradient / FlushLevel, "灯光渐变");
		Sys.SetTask(_task, true, 0);
	}
	else
	{
		for(int i=0; i<4; i++)
			pwm.Pulse[i]	= vs[i];

		// 刷新数据
		pwm.Flush();
	}
	debug_printf("开始调节……\r\n");
}

void Dimmer::Change(bool open)
{
	if(open)
		_Pwm->Open();
	else
		_Pwm->Close();
}

#if DEBUG
static void TestTask(void* param)
{
	static bool on	= true;

	byte vs1[]	= { 0, 0, 0, 0 };
	byte vs2[]	= { 0xFF, 0xFF, 0xFF, 0xFF };

	auto dim	= (Dimmer*)param;
	dim->Set(on ? vs1 : vs2);

	on	= !on;
}
#endif

void Dimmer::Test()
{
#if DEBUG
	Sys.AddTask(TestTask, this, 1000, 3000, "调光测试");
#endif
}
