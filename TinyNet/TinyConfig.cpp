#include "TinyConfig.h"

// 初始化
/*TinyConfig::TinyConfig()
{
	// 实际内存大小，减去头部大小
	uint len = sizeof(this) - ((int)&Length - (int)this);
	//memset(&Length, 0, len);
	Length = len;
}*/

void TinyConfig::LoadDefault()
{
	Kind	= Sys.Code;
	//Server	= 0x01;

	PingTime	= 15;
	OfflineTime	= 60;
}

void TinyConfig::Write(Stream& ms)const
{
	ms.Write((byte *)this, 0, sizeof(this[0]));
}

void TinyConfig::Read(Stream& ms)
{
	memcpy((byte *)this, ms.GetBuffer(), sizeof(this[0]));
}
