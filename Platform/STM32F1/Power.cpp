#include "Device\Power.h"

#include "Device\WatchDog.h"

#include "Platform\stm32.h"

void Power::OnStop()
{
	PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
}

void Power::OnDeepSleep()
{
#ifdef STM32F0
	PWR_EnterSleepMode(PWR_SLEEPEntry_WFI);
#endif
}

void Power::OnStandby()
{
	PWR_EnterSTANDBYMode();
}
