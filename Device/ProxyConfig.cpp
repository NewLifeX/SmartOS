
#include "ProxyConfig.h"


ProxyConfig::ProxyConfig(cstring name)
{
	_Name = "TokenCf";
	if (name)_Name = name;
	_Start = &Length;
	_End = &TagEnd;
	Init();
}

void ProxyConfig::Init()
{
	AutoStart	= false;		// 自动启动
	CacheSize	= 256;			// 缓存大小
	EnableStamp = false;		// 时间戳开关
	Buffer(PortCfg, sizeof(PortCfg)).Clear();
}
