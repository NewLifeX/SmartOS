
#include "BootConfig.h"

BootConfig* BootConfig::Current = nullptr;

BootConfig::BootConfig() :ConfigBase()
{
	_Name = "Boot";
	_Start = &Stat;
	_End = &TagEnd;

	Init();
}

void BootConfig::Init()
{
	ConfigBase::Init();

	Stat.HasApp = 0;				// 没有APP位置固件
	Stat.NeedUpDate = 0;			// 不需要升级

	App.WorkeAddr = 0x8010000;		// 64KB位置
	App.Depositary = 0x8010000;		// 64KB位置
	App.Length = 0x30000;			// 192KB
	App.Checksum = 0xffffffff;		// 特殊判断不校验
}

BootConfig * BootConfig::Create()
{
	static BootConfig bc;
	if (!Current)
	{
		Current = &bc;
		bc.Init();
	}
	return &bc;
}

/****************** PinConfig ********************/

PinConfig* PinConfig::Current = nullptr;

PinConfig::PinConfig() :ConfigBase()
{
	_Name = "Boot";
	_Start = &AllPin;
	_End = &TagEnd;

	Init();
}

void PinConfig::Init()
{
	ConfigBase::Init();
	IsEff = 0;
}

PinConfig * PinConfig::Create()
{
	static PinConfig pc;
	if (!Current)
	{
		Current = &pc;
		pc.Init();
	}

	return &pc;
}
