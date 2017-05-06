#include "AP0103.h"

#include "Kernel\Task.h"

#include "Device\SerialPort.h"
#include "Device\WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

#include "Net\Dhcp.h"
#include "Net\DNS.h"

#include "TinyNet\TinyConfig.h"
#include "TokenNet\TokenController.h"

#include "App\FlushPort.h"

AP0103* AP0103::Current = nullptr;
static TokenClient*	Client = nullptr;	// 令牌客户端

AP0103::AP0103()
{
	LedPins.Add(PC6);
	LedPins.Add(PC7);
	LedPins.Add(PC8);
	ButtonPins.Add(PE9);
	ButtonPins.Add(PA1);

	AlarmObj	= nullptr;

	Nrf = nullptr;
	Server = nullptr;
	_GateWay = nullptr;

	Data = nullptr;
	Size = 0;

	Current = this;
}

void* AP0103::InitData(void* data, int size)
{
	// 启动信息
	auto hot = &HotConfig::Current();
	hot->Times++;

	data = hot->Next();
	if (hot->Times == 1)
	{
		Buffer ds(data, size);
		ds.Clear();
		ds[0] = size;
	}

	Data = data;
	Size = size;

#if DEBUG
	debug_printf("数据区：");
	Buffer(Data, Size).Show(true);
#endif

	return data;
}

/******************************** In Out ********************************/

void AP0103::InitLeds()
{
	for (int i = 0; i < LedPins.Count(); i++)
	{
		auto port = new OutputPort(LedPins[i]);
		port->Open();
		Leds.Add(port);
	}
}

/*static void ButtonOnpress(InputPort* port, bool down, void* param)
{
	// if (port->PressTime > 1000)
		AP0103::OnPress(port, down);
}*/

void AP0103::InitButtons(const Delegate2<InputPort&, bool>& press)
{
	for (int i = 0; i < ButtonPins.Count(); i++)
	{
		auto port = new InputPort(ButtonPins[i]);
		port->Invert = true;
		port->Index	= i;
		port->Press	= press;
		port->UsePress();
		port->Open();
		Buttons.Add(port);
	}
}

/******************************** Token ********************************/

NetworkInterface* AP0103::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto tc = TinyConfig::Create();
	W5500 * net = nullptr;

	if(tc->HardVer >=5)
		net = new W5500(Spi1, PE7, PB2);
	else
		net = new W5500(Spi1, PE8, PE7);
	if(!net->Open())
	{
		delete net;
		return nullptr;
	}

	net->SetLed(*Leds[0]);
	net->EnableDNS();
	net->EnableDHCP();

	return net;
}

NetworkInterface* AP0103::Create8266(bool apOnly)
{
	auto esp = new Esp8266();
	esp->Init(COM4);
	esp->Set(P0, P0);

	// 初次需要指定模式 否则为 Wire
	bool join = esp->SSID && *esp->SSID;
	//if (!join) esp->Mode = NetworkType::AP;
	if (!join)
	{
		*esp->SSID = "WSWL";
		*esp->Pass = "12345678";

		esp->Mode = NetworkType::STA_AP;
	}

	if(!esp->Open())
	{
		delete esp;
		return nullptr;
	}

	esp->SetLed(*Leds[1]);
	Client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, esp);

	return esp;
}

void AP0103::InitClient()
{
	if (Client) return;

	// 初始化令牌网
	auto tk = TokenConfig::Create("smart.wslink.cn", NetType::Udp, 33333, 3377);

	// 创建客户端
	auto tc = TokenClient::CreateFast(Buffer(Data, Size));
	tc->Cfg = tk;
	tc->MaxNotActive = 8 * 60 * 1000;
	tc->UseLocal();

	Client = tc;
}

void AP0103::Register(int index, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(index, dp);
}

static void OnInitNet(void* param)
{
	auto& bsp	= *(AP0103*)param;

	bsp.Create5500();
	bsp.Create8266(false);

	Client->Open();
}

void AP0103::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
}

static void OnAlarm(AlarmItem& item)
{
	// 1长度n + 1类型 + 1偏移 + (n-2)数据
	auto bs	= item.GetData();
	debug_printf("OnAlarm ");
	bs.Show(true);

	if(bs[1] == 0x06)
	{
		auto client = Client;
		client->Store.Write(bs[2], bs.Sub(3, bs[0] - 2));

		// 主动上报状态
		client->ReportAsync(bs[2], bs[0] - 2);
	}
}

void AP0103::InitAlarm()
{
	if (!Client)return;

	if (!AlarmObj) AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::Set, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::Get, AlarmObj);

	AlarmObj->OnAlarm	= OnAlarm;
	AlarmObj->Start();
}


/******************************** Tiny *******************************/

int AP0103::Fix2401(const Buffer& bs)
{
	//auto& bs	= *(Buffer*)param;
	// 微网指令特殊处理长度
	uint rs = bs.Length();
	if (rs >= 8)
	{
		rs = bs[5] + 8;
		//if(rs < bs.Length()) bs.SetLength(rs);
	}
	return rs;
}

ITransport* AP0103::Create2401()
{
	//(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
	debug_printf("\r\n Create2401 \r\n");

	auto tc = TinyConfig::Create();

	static Spi spi(Spi2, 10000000, true);
	static NRF24L01 nrf;

	auto irq = tc->HardVer == 0x03 ? P0 : PD8;
	nrf.Init(&spi, PD9, irq, PE4);

	if (tc->Channel == 0)
	{
		tc->Channel = 120;
		tc->Speed = 250;
	}
	if (tc->Interval == 0)
	{
		tc->Interval = 40;
		tc->Timeout = 1000;
	}

	nrf.AutoAnswer = false;
	nrf.DynPayload = false;
	nrf.Channel = tc->Channel;
	nrf.Speed = tc->Speed;
	nrf.FixData = Fix2401;

	if (!nrf.Led) nrf.SetLed(*Leds[2]);

	nrf.Master = true;

	return &nrf;
}

void AP0103::InitTinyServer()
{
	if (Server)return;

	if(!Nrf) Nrf = Create2401();

	if (Nrf == nullptr)
	{
		debug_printf("Create2401 失败\r\n");
		return;
	}

	auto TinyCtl = new TinyController();

	TinyCtl->Port = Nrf;
	TinyCtl->QueueLength = 64;
	TinyCtl->ApplyConfig();

	// 新配置需要保存一下
	auto tc = TinyConfig::Current;
	if (tc == nullptr)TinyConfig::Create();

	if (tc && tc->New) tc->Save();

	Server = new TinyServer(TinyCtl);
	Server->Cfg = tc;
}

/*************************** Tiny && Token* **************************/

void AP0103::CreateGateway()
{
	if (!_GateWay)
	{
		if (!Client || !Server)debug_printf("TokenClient Or Server Not Ready!!\r\n");
		_GateWay = Gateway::CreateGateway(Client,Server);
	}
}

/******************************** 辅助 *******************************/

void AP0103::Restore()
{
	debug_printf("系统重置\r\n清空所有配置\r\n");
	auto bsp = Current;
	if (bsp->Server)bsp->Server->ClearDevices();
	if (TokenConfig::Current)TokenConfig::Current->Clear();
	if (TinyConfig::Current)TokenConfig::Current->Clear();

	if(Client) Client->Reset("按键重置");
}

void AP0103::OnPress(InputPort* port, bool down)
{
	if (down)return;
	ushort time = port->PressTime;
	if (time > 1000)
	{
		OnLongPress(port,down);
	}
}

void AP0103::OnLongPress(InputPort* port, bool down)
{
	if (down) return;
	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	ushort time = port->PressTime;
	auto client	= Client;
	if (time >= 5000 && time < 10000)
	{
		if(client) client->Reset("按键重置");
	}
	else if (time >= 3000)
	{
		if(client) client->Reboot("按键重启");
		Sys.Reboot(1000);
	}
}

/*
NRF24L01+ 	(SPI2)		|	W5500		(SPI1)		|
PB12		NSS			|	PA4			NSS			|
PB13		CLK			|	PA5			SCK			|
PB14		MISO		|	PA6			MISO		|
PB15		MOSI		|	PA7			MOSI		|
PD8			IRQ			|	PE7			INT(IRQ)	|
PD9			CE			|	PB2			NET_NRST	|
PE4			POWER		|	P0			POWER		|

ESP8266
COM4
Power	null
rst		null

BUTTON
PE9			CLEAR
PA1	PA0		USE_KEY(原复位按钮)

LED		可以做呼吸灯效果
PC6			LED1		TIM8_CH1
PC7			LED2		TIM8_CH2
PC8			LED3		TIM8_CH3
*/
