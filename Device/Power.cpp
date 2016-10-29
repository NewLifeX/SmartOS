#include "Device\Power.h"

#include "Device\WatchDog.h"

// 低功耗处理器
static List<Power*>	_powers;

void Power::SetPower()
{
	AddPower(this);
}

void Power::ChangePower(int level) { }

void Power::Stop(uint msTime)
{
	debug_printf("Power::Stop Time=%d \r\n", msTime);

	if(!msTime) msTime = 0xFFFF;
	WatchDog::Start(msTime);
	
	OnStop();
}

void Power::DeepSleep(uint msTime)
{
	debug_printf("Power::DeepSleep Time=%d \r\n", msTime);

	OnDeepSleep();
}

void Power::Standby(uint msTime)
{
	debug_printf("Power::Standby Time=%d \r\n", msTime);

	for(int i=0; i<_powers.Count(); i++)
	{
		auto pwr	= _powers[i];
		if(pwr)
		{
			debug_printf("Power::LowPower 0x%p\r\n", pwr);
			pwr->ChangePower(msTime);
		}
	}
	
	if(!msTime) msTime = 0xFFFF;
	WatchDog::Start(msTime);
	
	OnStandby();
}

// 各模块向系统注册低功耗句柄，供系统进入低功耗前调用
void Power::AddPower(Power* power)
{
	debug_printf("Power::AddPower 0x%p\r\n", power);
	
	_powers.Add(power);
}
