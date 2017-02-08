#include "AP0802.h"

#include "Kernel\Task.h"

#include "Device\WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

#include "TokenNet\TokenController.h"
#include "..\TinyNet\TinyConfig.h"
#include "..\App\FlushPort.h"

#include "Device\RTC.h"

AP0802 * AP0802::Current = nullptr;
static TokenClient*	Client = nullptr;	// 令牌客户端

AP0802::AP0802()
{
	Host = nullptr;
	HostAP = nullptr;
	Client = nullptr;

	Data = nullptr;
	Size = 0;

	Current = this;

	HardwareVer = HardwareVerLast;
}

void AP0802::Init(ushort code, cstring name, COM message)
{
	auto& sys = (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

	// RTC 提取时间
	HardRTC::Start(false, false);

	// 初始化系统
	sys.Init();
#if DEBUG
	sys.MessagePort = message; // 指定printf输出的串口
	Sys.ShowInfo();

	WatchDog::Start(20000, 10000);
#else
	WatchDog::Start();

	// 系统休眠时自动进入低功耗
	Power::AttachTimeSleep();
#endif

	// Flash最后一块作为配置区
	Config::Current = &Config::CreateFlash();

	if (HardwareVer == HardwareVerLast)
	{
		LedPins.Add(PE5);
		LedPins.Add(PE4);
		LedPins.Add(PD0);

		ButtonPins.Add(PE9);
		ButtonPins.Add(PE14);
	}

	if (HardwareVer <= HardwareVerAt160712)
	{
		LedPins.Add(PE5);
		LedPins.Add(PE4);
		LedPins.Add(PD0);

		ButtonPins.Add(PE13);
		ButtonPins.Add(PE14);
	}
}

void* AP0802::InitData(void* data, int size)
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

void AP0802::InitLeds()
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
	if (port->PressTime > 1000)
		AP0802::OnLongPress(port, down);
}

static void OnAlarm(AlarmItem& item)
{
	// 1长度n + 1类型 + 1偏移 + (n-2)数据
	auto bs = item.GetData();
	debug_printf("OnAlarm ");
	bs.Show(true);

	if (bs[1] == 0x06)
	{
		auto client = Client;
		client->Store.Write(bs[2], bs.Sub(3, bs[0] - 2));

		// 主动上报状态
		client->ReportAsync(bs[2], bs[0] - 2);
	}
}

void AP0802::InitAlarm()
{
	if (!Client)return;

	if (!AlarmObj)AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::Set, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::Get, AlarmObj);

	AlarmObj->OnAlarm = OnAlarm;
	AlarmObj->Start();
}

void AP0802::InitButtons(const Delegate2<InputPort&, bool>& press)
{
	for (int i = 0; i < ButtonPins.Count(); i++)
	{
		auto port = new InputPort(ButtonPins[i]);
		port->Invert = true;
		port->Index = i;
		port->Press = press;
		port->UsePress();
		port->Open();
		Buttons.Add(port);
	}
}

NetworkInterface* AP0802::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto host = new W5500(Spi2, PE1, PD13);

	return host;
}

NetworkInterface* AP0802::Create8266(bool apOnly)
{
	auto host = new Esp8266(COM4, PE0, PD3);

	// 初次需要指定模式 否则为 Wire
	bool join = host->SSID && *host->SSID;
	//if (!join) host->Mode = NetworkType::AP;
	if (!join)
	{
		*host->SSID = "WSWL";
		*host->Pass = "12345678";

		host->Mode = NetworkType::STA_AP;
		host->WorkMode = NetworkType::STA_AP;
	}

	Client->Register("SetWiFi", &Esp8266::SetWiFi, host);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, host);

	host->Open();

	return host;
}

/******************************** Token ********************************/

void AP0802::InitClient()
{
	if (Client) return;

	auto tk = TokenConfig::Current;

	// 创建客户端
	auto client = new TokenClient();
	client->Cfg = tk;

	// 需要使用本地连接
	//client->UseLocal();
	//配置闹钟
	InitAlarm();

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
		client, 8 * 60 * 1000, -1, "联网检查");
}

void AP0802::Register(int index, IDataPort& dp)
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
	auto& bsp = *(AP0802*)param;

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

void AP0802::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
}

/******************************** 2401 ********************************/


int Fix2401(const Buffer& bs)
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

ITransport* AP0802::Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
{
	debug_printf("\r\n Create2401 \r\n");

	static Spi spi(spi_, 10000000, true);
	static NRF24L01 nrf;
	nrf.Init(&spi, ce, irq, power);

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
	//nrf.Channel		= 120;
	nrf.Speed = tc->Speed;

	nrf.FixData = Fix2401;

	//if(WirelessLed) net->Led	= CreateFlushPort(WirelessLed);

	nrf.Master = true;

	return &nrf;
}

ITransport* AP0802::Create2401()
{
	auto port = new FlushPort();
	port->Port = Leds[2];
	return Create2401(Spi3, PD12, PE3, PE6, true, port);
}

void AP0802::Restore()
{
	if (Client) Client->Reset("按键重置");
}

void AP0802::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	ushort time = port->PressTime;
	auto client =Client;
	if (time >= 5000 && time < 10000)
	{
		if (client) client->Reset("按键重置");
	}
	else if (time >= 3000)
	{
		if (client) client->Reboot("按键重启");
		Sys.Reboot(1000);
	}
}

/*
网络使用流程：

5500网线检测
网线连通
	启动DHCP
	作为Host
Host为空 或 AP/STA_AP
	创建8266，加载配置
	Host为空
		作为Host
	else STA_AP
		工作模式 = AP
	打开8266
	STA/STA_AP
		SSID != null
			JoinWiFi
		else
			工作模式 = AP
	AP/STA_AP
		SetAP

令牌客户端主通道
令牌客户端内网通道
*/

/*
NRF24L01+ 	(SPI3)		|	W5500		(SPI2)
NSS						|				NSS
CLK						|				SCK
MISO					|				MISO
MOSI					|				MOSI
PE3			IRQ			|	PE1			INT(IRQ)
PD12		CE			|	PD13		NET_NRST
PE6			POWER		|				POWER

ESP8266		(COM4)
TX
RX
PD3			RST
PE2			POWER

				KEY1
				KEY2

				LED1
				LED2
				LED3

USB
PA11 PA12
*/
