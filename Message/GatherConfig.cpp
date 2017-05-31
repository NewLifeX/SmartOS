#include "Kernel\Sys.h"

#include "Device\Flash.h"

#include "GatherConfig.h"

GatherConfig::GatherConfig()
{
	_Name	= "Gather";
	_Start	= &RenderPeriod;
	_End	= &TagEnd;
}

void GatherConfig::Init()
{
	ConfigBase::Init();

	RenderPeriod	= 30;
	ReportPeriod	= 300;
	StorePeriod		= 600;

	MaxCache	= 16 * 1024;
	MaxReport	= 1024;
}

bool GatherConfig::Set(const Pair& args, Stream& result)
{
	debug_printf("GatherConfig::Set\r\n");

	auto cfg	= GatherConfig::Create();

	args.Get("RenderPeriod",	cfg->RenderPeriod);
	args.Get("ReportPeriod",	cfg->ReportPeriod);
	args.Get("StorePeriod",		cfg->StorePeriod);

	args.Get("MaxCache",		cfg->MaxCache);
	args.Get("MaxReport",		cfg->MaxReport);

	return true;
}

bool GatherConfig::Get(const Pair& args, Stream& result)
{
	debug_printf("GatherConfig::Get\r\n");

	auto cfg	= GatherConfig::Create();
	cfg->Write(result);

	return true;
}

GatherConfig* GatherConfig::Current;
GatherConfig* GatherConfig::Create()
{
	static GatherConfig tc;
	if(!Current)
	{
		GatherConfig::Current = &tc;
		tc.Init();
		tc.Load();
	}

	return &tc;
}
