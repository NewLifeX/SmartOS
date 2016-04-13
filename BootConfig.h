#ifndef __BootConfig__H__
#define __BootConfig__H__

#include "Sys.h"
#include "Config.h"


#pragma pack(push)
#pragma pack(1)

typedef struct
{
	uint HasApp : 1;		// 有可运行APP
	uint NeedUpDate : 1;	// 需要跟新
	uint Reserved : 30;		// 保留
}BootStat;

typedef struct
{
	uint WorkeAddr;		// 运行地址
	uint Depositary;	// 存放地址
	uint Length;		// 固件大小
	uint Checksum;		// 校验码
}FirmwareInfo;

#pragma pack(pop)

class BootConfig : public ConfigBase
{
public:
	BootStat Stat;
	FirmwareInfo App;
	FirmwareInfo Update;
	byte	TagEnd;		// 数据区结束标识符

	BootConfig();
	virtual void Init();
	// virtual void Load();
	// virtual void Save()const;

	static BootConfig* Current;
	static BootConfig* Create();
};


/****************** PinConfig ********************/


#pragma pack(push)
#pragma pack(1)

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
	W5500Pin W5500;
	NRFPin Nrf;
	Pin	UserKey1;		// 默认重启
	Pin	UserKey2;		// 默认设置
	Pin UserLed1;		// 默认蓝色
	Pin UserLed2;		// 默认红色
}PINS;

#pragma pack(pop)

class PinConfig : public ConfigBase
{
public:
	PINS AllPin;
	uint IsEff;
	byte	TagEnd;		// 数据区结束标识符

	PinConfig();
	virtual void Init();
	// virtual void Load();
	// virtual void Save()const;

	static PinConfig* Current;
	static PinConfig* Create();
};

#endif
