#ifndef __BootConfig__H__
#define __BootConfig__H__

#include "Kernel\Sys.h"
#include "Config.h"

#pragma pack(push)
#pragma pack(1)

typedef struct
{
	uint HasApp : 1;		// 有可运行APP
	uint NeedUpDate : 1;	// 需要跟新
	uint SearchPinCfg : 1;	// 搜索 PinCfg 固件
	uint Reserved : 29;		// 保留
}BootStat;

typedef struct
{
	uint WorkAddr;		// 运行地址
	uint Directory;		// 存放地址
	uint Length;		// 固件大小
	uint Checksum;		// 校验码
}FirmwareInfo;



typedef struct
{
	SPI _spi;
	Pin irq;
	Pin rst;
	Pin power;
	bool powerInvert;
	Pin	led;
}W5500Pin;

typedef struct
{
	SPI _spi;
	Pin ce;
	Pin irq;
	Pin power;
	bool powerInvert;
	Pin	led;
}NRFPin;

typedef struct
{
	COM MsgPort;
	int MsgPortBaudRate;
	W5500Pin W5500;
	NRFPin Nrf;
	Pin	UserKey1;		// 默认重启
	Pin	UserKey2;		// 默认设置
	Pin UserLed1;		// 默认蓝色
	Pin UserLed2;		// 默认红色
	uint IsEff;			// 是有效的配置
}PINS;

#pragma pack(pop)

class BootConfig : public ConfigBase
{
public:
	BootStat Stat;
	FirmwareInfo App;
	FirmwareInfo Update;

	PINS AllPin;
	byte	TagEnd;		// 数据区结束标识符

	BootConfig();
	virtual void Init();
#if defined DEBUG
	virtual void Show() const;
#endif
	static BootConfig* Current;
	static BootConfig* Create();
};

#endif
