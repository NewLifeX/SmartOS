#include "BaseBoard.h"

#include "Config.h"

#include "Device\RTC.h"
#include "Device\Power.h"
#include "Device\WatchDog.h"
#include "Device\SerialPort.h"

BaseBoard::BaseBoard()
{
	LedInvert = 2;
}

void BaseBoard::Init(ushort code, cstring name, int baudRate)
{
	auto& sys = (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

	// RTC 提取时间
	HardRTC::Start(false, false);

	// 初始化系统
	sys.Init();

	auto hot = &HotConfig::Current();
	// 热启动次数
	Sys.HotStart = hot->Times + 1;

#if DEBUG
	// 重新设置波特率
	if (baudRate > 0) {
		auto sp = SerialPort::GetMessagePort();
		if (sp) {
			sp->Close();
			sp->SetBaudRate(baudRate);
			sp->Open();
		}
	}

	Sys.ShowInfo();

	WatchDog::Start(20000, 10000);
#else
	WatchDog::Start();

	// 系统休眠时自动进入低功耗
	//Power::AttachTimeSleep();
#endif
}

// 初始化配置区
void BaseBoard::InitConfig()
{
	// Flash最后一块作为配置区
	Config::Current = &Config::CreateFlash();
}

void BaseBoard::InitLeds()
{
	for (int i = 0; i < LedPins.Count(); i++)
	{
		auto port = new OutputPort(LedPins[i], LedInvert);
		//auto port = new OutputPort(LedPins[i], false);
		port->Open();
		Leds.Add(port);
	}
}

void BaseBoard::InitButtons(const Delegate2<InputPort&, bool>& press)
{
	for (int i = 0; i < ButtonPins.Count(); i++)
	{
		auto port = new InputPort();
		port->Index = i;
		port->Set(ButtonPins[i]);
		//port->ShakeTime = 40;
		port->Press = press;
		port->UsePress();
		port->Open();

		Buttons.Add(port);
	}
}
