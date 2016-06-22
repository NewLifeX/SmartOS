#include "RTC.h"
#include "TTime.h"

/************************************************ HardRTC ************************************************/

HardRTC::HardRTC()
{
	LowPower	= true;
	External	= false;
	Opened		= false;
}

HardRTC* HardRTC::Instance()
{
	static HardRTC _rtc;

	return &_rtc;
}

void FuncLoadTicks() { HardRTC::Instance()->LoadTicks(); }
void FuncSaveTicks() { HardRTC::Instance()->SaveTicks(); }
int FuncSleep(int ms) { return HardRTC::Instance()->Sleep(ms); }

void HardRTC::Start(bool lowpower, bool external)
{
	auto& time	= (TTime&)Time;
	time.OnLoad		= FuncLoadTicks;
	time.OnSave		= FuncSaveTicks;
	time.OnSleep	= FuncSleep;

	auto rtc = Instance();
	if(!rtc->Opened) rtc->Init();
}
