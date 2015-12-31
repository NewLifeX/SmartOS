#include "TinyConfig.h"
#include "Config.h"

TinyConfig* TinyConfig::Current	= NULL;

TinyConfig::TinyConfig()
{
	Cfg	= Config::Current;
}

uint TinyConfig::Size() const
{
	return (uint)&Cfg - (uint)&Length;
}

Array TinyConfig::ToArray()
{
	return Array(&Length, Size());
}

const Array TinyConfig::ToArray() const
{
	return Array(&Length, Size());
}

void TinyConfig::LoadDefault()
{
	// 实际内存大小，减去头部大小
	uint len = Size();
	memset(&Length, 0, len);
	Length	= len;

	Kind	= Sys.Code;
	//Server	= 0x01;

	Channel	= 120;
	Speed	= 250;

	PingTime	= 20;
	OfflineTime	= 60;
}

void TinyConfig::Load()
{
	if(!Cfg) return;

	// 尝试加载配置区设置
	auto bs	= ToArray();
	if(!Cfg->GetOrSet("TCFG", bs))
		debug_printf("TinyConfig::Load 首次运行，创建配置区！\r\n");
	else
		debug_printf("TinyConfig::Load 从配置区加载配置\r\n");

	if(Kind != Sys.Code)
	{
		debug_printf("TinyConfig::Load 设备类型变更\r\n");

		Kind	= Sys.Code;
		Cfg->Set("TCFG", bs);
	}
}

void TinyConfig::Save() const
{
	if(!Cfg) return;

	debug_printf("TinyConfig::Save \r\n");

	Cfg->Set("TCFG", ToArray());
}

void TinyConfig::Clear()
{
	if(!Cfg) return;

	LoadDefault();

	debug_printf("TinyConfig::Clear \r\n");

	Cfg->Set("TCFG", ToArray());
}

void TinyConfig::Write(Stream& ms) const
{
	ms.Write(ToArray());
}

void TinyConfig::Read(Stream& ms)
{
	auto bs	= ToArray();
	ms.Read(bs);
}

TinyConfig* TinyConfig::Init()
{
	// 默认出厂设置
	static TinyConfig tc;
	TinyConfig::Current = &tc;
	tc.LoadDefault();

	return &tc;
}
