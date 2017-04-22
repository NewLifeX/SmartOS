#include "Device\Power.h"

#include "Device\WatchDog.h"

#include "Platform\stm32.h"

void Power::OnSleep(int msTime)
{
	PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFE);
}

void Power::OnStop(int msTime)
{
#ifdef STM32F0
	PWR_EnterSleepMode(PWR_SLEEPEntry_WFI);
#endif
}

void Power::OnStandby(int msTime)
{
	if(!msTime) msTime = 20000;
	WatchDog::Start(msTime);

	PWR_EnterSTANDBYMode();
}
