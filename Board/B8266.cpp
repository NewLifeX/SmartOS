#include "B8266.h"

#include "Drivers\Esp8266\Esp8266.h"

B8266* B8266::Current = nullptr;

B8266::B8266()
{
	LedPins.Add(PA4);
	LedPins.Add(PA5);

	Esp.Com = COM2;
	Esp.Baudrate = 115200;
	Esp.Power = PB2;
	Esp.Reset = PA1;
	Esp.LowPower = P0;

	SSID = "WSWL";
	Pass = "12345678";

	Current = this;
}

void B8266::InitWiFi(cstring ssid, cstring pass)
{
	SSID = ssid;
	Pass = pass;
}

NetworkInterface* B8266::Create8266()
{
	auto esp = new Esp8266();
	esp->Init(Esp.Com, Esp.Baudrate);
	esp->Set(Esp.Power, Esp.Reset, Esp.LowPower);

	// 初次需要指定模式 否则为 Wire
	bool join = esp->SSID && *esp->SSID;
	//if (!join) esp->Mode = NetworkType::AP;

	if (!join)
	{
		*esp->SSID = SSID;
		*esp->Pass = Pass;

		esp->Mode = NetworkType::STA_AP;
	}

	if (!esp->Open())
	{
		delete esp;
		return nullptr;
	}

	//esp->SetLed(*Leds[0]);
	Client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, esp);
	Client->Register("GetAPs", &Esp8266::GetAPs, esp);

	return esp;
}

void B8266::InitNet()
{
	Create8266();
	Client->Open();
}

static void OnAlarm(AlarmItem& item)
{
	// 1长度n + 1类型 + 1偏移 + (n-2)数据
	auto bs = item.GetData();
	debug_printf("OnAlarm ");
	bs.Show(true);

	if (bs[1] == 0x06)
	{
		auto client = B8266::Current->Client;
		client->Store.Write(bs[2], bs.Sub(3, bs[0] - 2));

		// 主动上报状态
		client->ReportAsync(bs[2], bs[0] - 2);
	}
}

void B8266::InitAlarm()
{
	if (!Client)return;

	if (!AlarmObj) AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::Set, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::Get, AlarmObj);

	AlarmObj->OnAlarm = OnAlarm;
	AlarmObj->Start();
}

static void RestPress(InputPort& port, bool down)
{
	if (down) return;
	auto client = B8266::Current;
	client->Restore();
}

void B8266::SetRestore(Pin pin)
{
	if (pin == P0) return;

	auto port = new InputPort(pin);
	port->Open();
	port->UsePress();
	port->Press = RestPress;
	RestPort = port;
}

/*
NRF24L01+ 	(SPI3)		|	W5500		(SPI2)		|	TOUCH		(SPI3)
NSS			|				NSS			|	PD6			NSS
CLK			|				SCK			|				SCK
MISO		|				MISO		|				MISO
MOSI		|				MOSI		|				MOSI
PE3			IRQ			|	PE1			INT(IRQ)	|	PD11		INT(IRQ)
PD12		CE			|	PD13		NET_NRST	|				NET_NRST
PE6			POWER		|				POWER		|				POWER

ESP8266		(COM4)
TX
RX
PD3			RST
PE2			POWER

TFT
FSMC_D 0..15		TFT_D 0..15
NOE					RD
NWE					RW
NE1					RS
PE4					CE
PC7					LIGHT
PE5					RST

PE13				KEY1
PE14				KEY2

PE15				LED2
PD8					LED1


USB
PA11 PA12
*/
