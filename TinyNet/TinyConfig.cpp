#include "TinyConfig.h"
#include "Config.h"

TinyConfig* TinyConfig::Current	= nullptr;

TinyConfig::TinyConfig() : ConfigBase(),
	Pass(_Pass, sizeof(_Pass))
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

	Pass.SetLength(_PassLen);
}

void TinyConfig::Load()
{
	ConfigBase::Load();

	Pass.SetLength(_PassLen);
}

void TinyConfig::Save() const
{
	auto cfg	= (TinyConfig*)this;
	cfg->_PassLen	= Pass.Length();

	ConfigBase::Save();
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
