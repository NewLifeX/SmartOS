#include "Config.h"

TConfig Config;

// 初始化，各字段为0
void TConfig::Init()
{
	// 实际内存大小，减去头部大小
	uint len = sizeof(this) - (int)&Length - (int)this;
	memset(&Length, 0, len);
	Length = len;
}

void TConfig::LoadDefault()
{
	Type	= Sys.Code;
}
