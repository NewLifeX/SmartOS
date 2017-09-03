#include "Esp8266Module.h"

#include "Drivers\Esp8266\Esp8266.h"

Esp8266Module::Esp8266Module()
{
	SSID = "WSWL";
	Pass = "12345678";

	Esp.Com = COM4;
	Esp.Baudrate = 115200;
	Esp.Power = PE2;
	Esp.Reset = PD3;
	Esp.LowPower = P0;
}

void Esp8266Module::InitWiFi(cstring ssid, cstring pass)
{
	SSID = ssid;
	Pass = pass;
}

NetworkInterface* Esp8266Module::Create8266(OutputPort* led)
{
	debug_printf("\r\nEsp8266::Create \r\n");

	auto esp = new Esp8266();
	esp->Init(Esp.Com, Esp.Baudrate);
	esp->Set(Esp.Power, Esp.Reset, Esp.LowPower);

	// 初次需要指定模式 否则为 Wire
	bool join = esp->SSID && *esp->SSID;
	if (!join)
	{
		*esp->SSID = SSID;
		*esp->Pass = Pass;

		esp->Mode = NetworkType::STA_AP;
		esp->WorkMode = NetworkType::STA_AP;
	}

	if (!esp->Open())
	{
		delete esp;
		return nullptr;
	}

	if (led)esp->SetLed(*led);
	//Client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
	//Client->Register("GetWiFi", &Esp8266::GetWiFi, esp);
	//Client->Register("GetAPs", &Esp8266::GetAPs, esp);

	return esp;
}
