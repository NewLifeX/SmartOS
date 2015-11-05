#include "TinyConfig.h"
#include "Config.h"

TinyConfig* TinyConfig::Current	= NULL;

void TinyConfig::LoadDefault()
{
	// 实际内存大小，减去头部大小
	uint len = sizeof(this[0]) - ((int)&Length - (int)this);
	memset(&Length, 0, len);
	Length = len;

	Kind	= Sys.Code;
	//Server	= 0x01;

	PingTime	= 10;
	OfflineTime	= 60;
	StartSet	= 64;
}

void TinyConfig::Load()
{
	// Flash最后一块作为配置区
	if(!Config::Current) Config::Current = &Config::CreateFlash();

	// 尝试加载配置区设置
	uint len = Length;
	if(!len) len = sizeof(this[0]);
	ByteArray bs(&Length, len);
	if(!Config::Current->GetOrSet("TCFG", bs))
		debug_printf("TinyConfig::Load 首次运行，创建配置区！\r\n");
	else
		debug_printf("TinyConfig::Load 从配置区加载配置\r\n");

	if(Kind != Sys.Code)
	{
		debug_printf("TinyConfig::Load 设备类型变更\r\n");

		Kind	= Sys.Code;
		Config::Current->Set("TCFG", bs);
	}
}

void TinyConfig::Save()
{
	uint len = Length;
	if(!len) len = sizeof(this[0]);

	debug_printf("TinyConfig::Save \r\n");

	ByteArray bs(&Length, len);
	Config::Current->Set("TCFG", bs);
}

void TinyConfig::Clear()
{
	LoadDefault();

	debug_printf("TinyConfig::Clear \r\n");

	ByteArray bs(&Length, Length);
	Config::Current->Set("TCFG", bs);
}

void TinyConfig::Write(Stream& ms)const
{
	ms.Write((byte *)this, 0, sizeof(this[0]));
}

void TinyConfig::Read(Stream& ms)
{
	memcpy((byte *)this, ms.GetBuffer(), sizeof(this[0]));
}

TinyConfig* TinyConfig::Init()
{
	// 默认出厂设置
	static TinyConfig tc;
	TinyConfig::Current = &tc;
	tc.LoadDefault();
	tc.Channel	= 120;
	tc.Speed	= 250;

	return &tc;
}
