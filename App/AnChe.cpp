#include "Kernel\Sys.h"

#include "Device/Pwm.h"

#include "Config.h"

#include "AnChe.h"

AnCheConfig::AnCheConfig()
{
	_Name = "AnChe";
	_Start = &Anche;
	_End = &TagEnd;
	Init();
	
	Serial1Power = true;		// 串口2开关状态
	BaudRate1 = 9600;			// 串口2波特率
	DataBits1 = 8;				// 串口2数据位
	Parity1 = 0;				// 串口2奇偶校验位
	StopBits1 = 1;				// 串口2停止位
	Serial2Power = true;		// 串口3开关状态
	BaudRate2 = 9600;			// 串口3波特率
	DataBits2 = 8;				// 串口3数据位
	Parity2 = 0;				// 串口3奇偶校验位
	StopBits2 = 1;				// 串口3停止位
	IsInfrare=false;			// 是否红外控制
	InitWeight[0] = 80;			// 初始重量变化阈值
	InitWeight[1] = 80;			// 初始重量变化阈值
	StableWeight[0] = 80;		// 稳定重量变化阈值	
	StableWeight[1] = 80;		// 稳定重量变化阈值	

}

static AnCheConfig* _cfg = nullptr;
static ByteArray	_bak;
static uint			_saveTask = 0;

static void SaveTask(void* param)
{
	if (!_cfg) return;

	// 比较数据，如果不同则保存
	auto bs = _cfg->ToArray();
	if (_bak.Length() == 0 || bs != _bak)
	{
		_bak = bs;
		_cfg->Save();
	}
}

static AnCheConfig* InitConfig()
{
	static AnCheConfig	cfg;
	if (!_cfg)
	{
		_cfg = &cfg;
		_cfg->Load();

		_bak = cfg.ToArray();
		if (!_saveTask) _saveTask = Sys.AddTask(SaveTask, nullptr, 1000, 10000, "保存配置");
	}

	return _cfg;
}

void AnChe::GetConfig()
{
	Config = InitConfig();
}

