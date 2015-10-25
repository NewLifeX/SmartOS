#include "TinyConfig.h"

// 初始化
/*TinyConfig::TinyConfig()
{
	// 实际内存大小，减去头部大小
	uint len = sizeof(this) - ((int)&Length - (int)this);
	//memset(&Length, 0, len);
	Length = len;
}*/

/*const TinyConfig& Default()
{
	const TinyConfig tc =
	{
		sizeof(TinyConfig),

		1,
		1,

		10,
		60,
		5,

		120,
		250,
		60,
	};

	return tc;
}*/

void TinyConfig::LoadDefault()
{
	// 实际内存大小，减去头部大小
	uint len = sizeof(this) - ((int)&Length - (int)this);
	memset(&Length, 0, len);
	Length = len;

	Kind	= Sys.Code;
	//Server	= 0x01;

	PingTime	= 10;
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
