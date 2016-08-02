#include "AP0104.h"

#include "Task.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

#include "Net\Dhcp.h"
#include "Net\DNS.h"

#include "TinyNet\TinyConfig.h"
#include "TokenNet\TokenController.h"

#include "App\FlushPort.h"

// static FlushPort* CreateFlushPort(OutputPort* led)
// {
// 	auto fp	= new FlushPort();
// 	fp->Port	= led;
// 	fp->Start();
// 
// 	return fp;
// }

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

	Data = nullptr;
	Size = 0;
}

void AP0104::Init(ushort code, cstring name, COM message)
{
	auto& sys = (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

	// 初始化系统
	sys.Init();
#if DEBUG
	sys.MessagePort = message; // 指定printf输出的串口
	Sys.ShowInfo();

	WatchDog::Start(20000, 10000);
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

void AP0104::InitLeds()
{
	for (int i = 0; i < LedPins.Count(); i++)
	{
		auto port = new OutputPort(LedPins[i]);
		port->Open();
		Leds.Add(port);
	}
}

void AP0104::InitButtons()
{
	for (int i = 0; i < ButtonPins.Count(); i++)
	{
		auto port = new InputPort(ButtonPins[i]);
		port->Mode = InputPort::Both;
		port->Invert = true;
		//port->Register(OnPress, (void*)i);
		port->Open();
		Buttons.Add(port);
	}
}

ISocketHost* AP0104::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto host = new W5500(Spi1, PE7, PB2);
	host->NetReady.Bind(&AP0104::OpenClient, this);

	return host;
}

ISocketHost* AP0104::Create8266(bool apOnly)
{
	auto host = new Esp8266(COM4, P0, P0);

	// 初次需要指定模式 否则为 Wire
	bool join = host->SSID && *host->SSID;
	//if (!join) host->Mode = SocketMode::AP;
	if (!join)
	{
		*host->SSID = "WsLink";
		host->Mode = SocketMode::STA_AP;
	}

	// 绑定委托，避免5500没有连接时导致没有启动客户端
	host->NetReady.Bind(&AP0104::OpenClient, this);

	//Sys.AddTask(SetWiFiTask, this, 0, -1, "SetWiFi");
	Client->Register("SetWiFi", &Esp8266::SetWiFi, host);

	host->OpenAsync();

	return host;
}

/******************************** Token ********************************/

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
			Sys.Reset();
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

void AP0104::OpenClient(ISocketHost& host)
{
	assert(Client, "Client");

	debug_printf("\r\n OpenClient \r\n");

	// 网络就绪后，打开指示灯
	auto net = dynamic_cast<W5500*>(&host);
	if (net && !net->Led) net->SetLed(*Leds[0]);

	auto esp = dynamic_cast<Esp8266*>(&host);
	if (esp && !esp->Led) esp->SetLed(*Leds[1]);

	auto tk = TokenConfig::Current;
	NetUri uri(NetType::Udp, IPAddress::Broadcast(), 3355);

	// 避免重复打开
	if (!Client->Opened && Host)
	{
		AddControl(*Host, tk->Uri(), 0);
		AddControl(*Host, uri, tk->Port);

		Client->Open();
	}

	if (HostAP && esp)
	{
		auto ctrl = AddControl(*HostAP, uri, tk->Port);

		// 如果没有主机，这里打开令牌客户端，为组网做准备
		if (!Host)
			Client->Open();
		else
			Client->AttachControls();

		// 假如来迟了，客户端已经打开，那么自己挂载事件
		if (Client->Opened && Client->Master)
		{
			ctrl->Received = Client->Master->Received;
			ctrl->Open();
		}
	}
}

TokenController* AP0104::AddControl(ISocketHost& host, const NetUri& uri, ushort localPort)
{
	// 创建连接服务器的Socket
	auto socket = host.CreateRemote(uri);

	// 创建连接服务器的控制器
	auto ctrl = new TokenController();
	//ctrl->Port = dynamic_cast<ITransport*>(socket);
	ctrl->Socket = socket;

	// 创建客户端
	auto client = Client;
	if (localPort == 0)
		client->Master = ctrl;
	else
	{
		socket->Local.Port = localPort;
		ctrl->ShowRemote = true;
		client->Controls.Add(ctrl);
	}

	return ctrl;
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
		auto esp = bsp.Create8266(false);
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
	// 接了网线，同时需要AP
	else if (host->IsAP())
	{
		// 如果Host已存在，则8266仅作为AP
		auto esp = bsp.Create8266(true);
		if (esp) bsp.HostAP = esp;
	}

	// 打开DHCP，完成时会打开客户端
	//if(bsp.Host) bsp.Host->EnableDHCP();
}

void AP0104::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
}


/******************************** 2401 *******************************

int AP0104::Fix2401(const Buffer& bs)
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

ITransport* AP0104::Create2401()//(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
{
debug_printf("\r\n Create2401 \r\n");

static Spi spi(Spi2, 10000000, true);
static NRF24L01 nrf;
nrf.Init(&spi, PD9, PD8, PE4);

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

if(WirelessLed) nrf.Led	= CreateFlushPort(WirelessLed);

nrf.Master	= true;

return &nrf;
}
*/


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
