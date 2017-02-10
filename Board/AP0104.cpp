#include "AP0104.h"

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

AP0104* AP0104::Current = nullptr;

AP0104::AP0104()
{
	LedPins.Add(PC6);
	LedPins.Add(PC7);
	LedPins.Add(PC8);
	ButtonPins.Add(PE9);
	ButtonPins.Add(PA1);

	Host = nullptr;
	HostAP = nullptr;
	Client = nullptr;

	Nrf = nullptr;
	Server = nullptr;
	_GateWay = nullptr;

	Data = nullptr;
	Size = 0;

	Current = this;
}

void AP0104::Init(ushort code, cstring name, COM message)
{
	auto& sys = (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

	// RTC 提取时间
	auto Rtc = HardRTC::Instance();
	Rtc->LowPower = false;
	Rtc->External = false;
	Rtc->Init();
	Rtc->Start(false, false);

	// 初始化系统
	sys.Init();
#if DEBUG
	sys.MessagePort = message; // 指定printf输出的串口
	Sys.ShowInfo();

	WatchDog::Start(20000, 10000);
#else
	WatchDog::Start();
#endif

		// Flash最后一块作为配置区
	Config::Current = &Config::CreateFlash();
}

void* AP0104::InitData(void* data, int size)
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

	return data;
}

/******************************** In Out ********************************/

void AP0104::InitLeds()
{
	for (int i = 0; i < LedPins.Count(); i++)
	{
		auto port = new OutputPort(LedPins[i]);
		port->Open();
		Leds.Add(port);
	}
}

void ButtonOnpress(InputPort* port, bool down, void* param)
{
	// if (port->PressTime > 1000)
		AP0104::OnPress(port, down);
}

void AP0104::InitButtons(const Delegate2<InputPort&, bool>& press)
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

NetworkInterface* AP0104::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto host = new W5500(Spi1, PE7, PB2);

	return host;
}

NetworkInterface* AP0104::Create8266(bool apOnly)
{
	auto host = new Esp8266(COM4, P0, P0);

	// 初次需要指定模式 否则为 Wire
	bool join = host->SSID && *host->SSID;
	//if (!join) host->Mode = NetworkType::AP;
	if (!join)
	{
		*host->SSID = "WSWL";
		*host->Pass = "12345678";

		host->Mode = NetworkType::STA_AP;
	}

	Client->Register("SetWiFi", &Esp8266::SetWiFi, host);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, host);

	host->Open();

	return host;
}

void AP0104::InitClient()
{
	if (Client) return;

	auto tk = TokenConfig::Current;

	// 创建客户端
	auto client = new TokenClient();
	client->Cfg = tk;

	// 需要使用本地连接
	//client->UseLocal();

	Client = client;
	Client->MaxNotActive = 480000;

	// 重启
	Client->Register("Gateway/Restart", &TokenClient::InvokeRestart, Client);
	// 重置
	Client->Register("Gateway/Reset", &TokenClient::InvokeReset, Client);
	// 设置远程地址
	Client->Register("Gateway/SetRemote", &TokenClient::InvokeSetRemote, Client);
	// 获取远程配置信息
	Client->Register("Gateway/GetRemote", &TokenClient::InvokeGetRemote, Client);
	// 获取所有Ivoke命令
	Client->Register("Api/All", &TokenClient::InvokeGetAllApi, Client);

	if (Data && Size > 0)
	{
		auto& ds = Client->Store;
		ds.Data.Set(Data, Size);
	}

	// 如果若干分钟后仍然没有打开令牌客户端，则重启系统
	Sys.AddTask(
		[](void* p) {
		auto& client = *(TokenClient*)p;
		if (!client.Opened)
		{
			debug_printf("联网超时，准备重启系统！\r\n\r\n");
			Sys.Reboot();
		}
	},
		client, 8 * 60 * 1000, -1, "check connet net");
}

void AP0104::Register(int index, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(index, dp);
}

/*
网络使用流程：

5500网线检测
网线连通
启动DHCP/DNS
作为Host
Host为空 或 AP/STA_AP
创建8266，加载配置
Host为空
作为Host
else STA_AP
工作模式 = AP

令牌客户端主通道
令牌客户端内网通道
*/
void OnInitNet(void* param)
{
	auto& bsp = *(AP0104*)param;

	// 检查是否连接网线
	auto host = (W5500*)bsp.Create5500();
	// 软路由的DHCP要求很严格，必须先把自己IP设为0
	host->IP = IPAddress::Any();
	if (host->Open())
	{
		host->EnableDNS();
		host->EnableDHCP();
		bsp.Host = host;
	}
	else
	{
		delete host;
	}

	// 没有接网线，需要完整WiFi通道
	if (!bsp.Host)
	{
		auto esp = (WiFiInterface*)bsp.Create8266(false);
		if (esp)
		{
			// 未组网时，主机留空，仅保留AP主机
			bool join = esp->SSID && *esp->SSID;
			if (join && esp->IsStation())
				bsp.Host = esp;
			else
				bsp.HostAP = esp;
		}
	}
}

void AP0104::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
}


/******************************** Tiny *******************************/

int AP0104::Fix2401(const Buffer& bs)
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

ITransport* AP0104::Create2401()
{
	//(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
	debug_printf("\r\n Create2401 \r\n");

	static Spi spi(Spi2, 10000000, true);
	static NRF24L01 nrf;
	nrf.Init(&spi, PD9, PD8, PE4);

	auto tc = TinyConfig::Create();
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

void AP0104::InitTinyServer()
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

void AP0104::CreateGateway()
{
	if (!_GateWay)
	{
		if (!Client || !Server)debug_printf("TokenClient Or Server Not Ready!!\r\n");
		_GateWay = Gateway::CreateGateway(Client,Server);
	}
}

/******************************** 辅助 *******************************/

void AP0104::Restore()
{
	debug_printf("系统重置\r\n清空所有配置\r\n");
	auto bsp = Current;
	if (bsp->Server)bsp->Server->ClearDevices();
	if (TokenConfig::Current)TokenConfig::Current->Clear();
	if (TinyConfig::Current)TokenConfig::Current->Clear();

	debug_printf("系统将在1秒后重启\r\n");
	if(Client) Client->Reset("按键重置");
}

void AP0104::OnPress(InputPort* port, bool down)
{
	if (down)return;
	ushort time = port->PressTime;
	if (time > 1000)
	{
		OnLongPress(port,down);
	}
}

void AP0104::OnLongPress(InputPort* port, bool down)
{
	if (down) return;
	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	ushort time = port->PressTime;
	auto client	= AP0104::Current->Client;
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
