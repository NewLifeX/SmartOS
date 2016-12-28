#include "RTC.h"
#include "Kernel\TTime.h"

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

void FuncLoadTime() { HardRTC::Instance()->LoadTime(); }
void FuncSaveTime() { HardRTC::Instance()->SaveTime(); }
void FuncSleep(int ms) { HardRTC::Instance()->Sleep(ms); }

void HardRTC::Start(bool lowpower, bool external)
{
	auto& time	= (TTime&)Time;
	time.OnLoad		= FuncLoadTime;
	time.OnSave		= FuncSaveTime;
	if(lowpower) time.OnSleep	= FuncSleep;

	auto rtc = Instance();
	if(!rtc->Opened)
	{
		rtc->LowPower	= lowpower;
		rtc->External	= external;
		rtc->Init();
	}
}
