
#include "BootConfig.h"

BootConfig* BootConfig::Current = nullptr;
const char bootName[] = {'B','o','o','t',   'C','f','g','\0'};

BootConfig::BootConfig() :ConfigBase()
{
	_Name = bootName;
	_Start = &Stat;
	_End = &TagEnd;

	Init();
}

void BootConfig::Init()
{
	ConfigBase::Init();

	Stat.HasApp = 0;				// 没有APP位置固件
	Stat.NeedUpDate = 0;			// 不需要升级
	Stat.SearchPinCfg = 1;			// 默认搜索引脚配置固件 PinCfg
#if defined DEBUG
	App.WorkAddr = 0x8020000;		// 128KB位置
	App.Directory = 0x8020000;		// 128KB位置
	App.Length = 0x20000;			// 128KB
#else
	App.WorkAddr = 0x8010000;		// 64KB位置
	App.Directory = 0x8010000;		// 64KB位置
	App.Length = 0x30000;			// 192KB
#endif
	App.Checksum = 0xffffffff;		// 特殊判断不校验

	AllPin.MsgPort = COM1;			// 默认COM
	AllPin.MsgPortBaudRate = 1024000; // 默认波特率
	AllPin.IsEff = 0;				// 不是有效的
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

#if defined DEBUG
void BootConfig::Show() const
{
	debug_printf("BootConfig   HasApp %d  NeedUpData %d  SearchPinCfg %d\r\n", Stat.HasApp, Stat.NeedUpDate, Stat.SearchPinCfg);
	debug_printf("WorkAddr %p  Length %p  Depositary %p  Checksum %p\r\n", App.WorkAddr, App.Length, App.Directory, App.Checksum);
	debug_printf("UpdateAddr %p  Length %p  Depositary %p  Checksum %p\r\n", Update.WorkAddr, Update.Length, Update.Directory, Update.Checksum);

	debug_printf("PinConfig\r\n");
	if (AllPin.IsEff)
	{
		debug_printf("MsgPort COM%d  BaudRate %d\r\n", AllPin.MsgPort + 1, AllPin.MsgPortBaudRate);
		debug_printf("W5500 SPI%d  \r\n", AllPin.W5500._spi + 1);
	}
	else
	{
		debug_printf("Pins is Not Eff !! \r\n");
	}
}
#endif
