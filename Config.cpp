#include "Config.h"

//TConfig Config;

// 初始化
TConfig::TConfig()
{
	// 实际内存大小，减去头部大小
	uint len = sizeof(this) - ((int)&Length - (int)this);
	//memset(&Length, 0, len);
	Length = len;
}

void TConfig::LoadDefault()
{
	Kind	= Sys.Code;
	//Server	= 0x01;
	
	PingTime	= 15;
	OfflineTime	= 60;
}
