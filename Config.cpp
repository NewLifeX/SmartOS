#include "Config.h"

TConfig Config;

// 初始化，各字段为0
TConfig::TConfig()
{
	// 实际内存大小，减去头部大小
	Length = sizeof(this) - (int)&Length - (int)this;
	memset(&Length, 0, Length);
}
