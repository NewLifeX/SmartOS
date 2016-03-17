#include "TinyConfig.h"
#include "Config.h"

TinyConfig* TinyConfig::Current	= nullptr;

TinyConfig::TinyConfig() : ConfigBase()
{
	_Name	= "TinyCfg";
	_Start	= &Length;
	_End	= &TagEnd;
	
	Init();
}

void TinyConfig::Init()
{
	ConfigBase::Init();
	
	Length	= Size();

	Kind	= Sys.Code;

	PingTime	= 20;
	OfflineTime	= 60;
}

TinyConfig* TinyConfig::Create()
{
	// 默认出厂设置
	static TinyConfig tc;
	if(!Current)
	{
		Current = &tc;
		tc.Init();
	}

	return &tc;
}
