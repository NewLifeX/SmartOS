#include "AP0801.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

#include "Message\ProxyFactory.h"

AP0801* AP0801::Current = nullptr;
static ProxyFactory*	ProxyFac = nullptr;	// 透传管理器

AP0801::AP0801()
{
	LedPins.Add(PD8);
	LedPins.Add(PE15);
	ButtonPins.Add(PE13);
	ButtonPins.Add(PE14);

	LedInvert = false;

	ProxyFac = nullptr;
	AlarmObj = nullptr;

	Net.Spi = Spi2;
	Net.Irq = PE1;
	Net.Reset = PD13;

	Esp.Com = COM4;
	Esp.Baudrate = 115200;
	Esp.Power = PE2;
	Esp.Reset = PD3;
	Esp.LowPower = P0;

	Current = this;
}

NetworkInterface* AP0801::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto net = new W5500(Net.Spi, Net.Irq, Net.Reset);
	if (!net->Open())
	{
		delete net;
		return nullptr;
	}

	net->SetLed(*Leds[0]);
	net->EnableDNS();
	net->EnableDHCP();

	return net;
}

NetworkInterface* AP0801::Create8266()
{
	debug_printf("\r\nEsp8266::Create \r\n");

	auto esp = new Esp8266();
	esp->Init(Esp.Com, Esp.Baudrate);
	esp->Set(Esp.Power, Esp.Reset, Esp.LowPower);

	// 初次需要指定模式 否则为 Wire
	bool join = esp->SSID && *esp->SSID;
	if (!join)
	{
		*esp->SSID = "WSWL";
		*esp->Pass = "12345678";

		esp->Mode = NetworkType::STA_AP;
		esp->WorkMode = NetworkType::STA_AP;
	}

	if (!esp->Open())
	{
		delete esp;
		return nullptr;
	}

	esp->SetLed(*Leds[1]);
	Client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, esp);

	return esp;
}

static void OnInitNet(void* param)
{
	auto& bsp = *(AP0801*)param;

	bsp.Create5500();
	bsp.Create8266();

	bsp.Client->Open();
}

void AP0801::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
}

void  AP0801::InitProxy()
{
	if (ProxyFac)return;
	if (!Client)
	{
		debug_printf("请先初始化TokenClient！！\r\n");
		return;
	}
	ProxyFac = ProxyFactory::Create();

	ProxyFac->Register(new ComProxy(COM2));

	ProxyFac->Open(Client);
	// ProxyFac->AutoStart();		// 自动启动的设备  需要保证Client已经开启，否则没有意义
}

static void OnAlarm(AlarmItem& item)
{
	// 1长度n + 1类型 + 1偏移 + (n-2)数据
	auto bs = item.GetData();
	debug_printf("OnAlarm ");
	bs.Show(true);

	if (bs[1] == 0x06)
	{
		auto client = AP0801::Current->Client;
		client->Store.Write(bs[2], bs.Sub(3, bs[0] - 2));

		// 主动上报状态
		client->ReportAsync(bs[2], bs[0] - 2);
	}
}

void AP0801::InitAlarm()
{
	if (!Client)return;

	if (!AlarmObj) AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::Set, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::Get, AlarmObj);

	AlarmObj->OnAlarm = OnAlarm;
	AlarmObj->Start();
}

/******************************** 2401 ********************************/

/*int Fix2401(const Buffer& bs)
{
	//auto& bs	= *(Buffer*)param;
	// 微网指令特殊处理长度
	uint rs	= bs.Length();
	if(rs >= 8)
	{
		rs = bs[5] + 8;
		//if(rs < bs.Length()) bs.SetLength(rs);
	}
	return rs;
}

ITransport* AP0801::Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
{
	debug_printf("\r\n Create2401 \r\n");

	static Spi spi(spi_, 10000000, true);
	static NRF24L01 nrf;
	nrf.Init(&spi, ce, irq, power);

	auto tc	= TinyConfig::Create();
	if(tc->Channel == 0)
	{
		tc->Channel	= 120;
		tc->Speed	= 250;
	}
	if(tc->Interval == 0)
	{
		tc->Interval= 40;
		tc->Timeout	= 1000;
	}

	nrf.AutoAnswer	= false;
	nrf.DynPayload	= false;
	nrf.Channel		= tc->Channel;
	//nrf.Channel		= 120;
	nrf.Speed		= tc->Speed;

	nrf.FixData	= Fix2401;

	if(WirelessLed) net->Led	= CreateFlushPort(WirelessLed);

	nrf.Master	= true;

	return &nrf;
}*/

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
