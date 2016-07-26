#include "AP0802.h"

#include "Task.h"

#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

#include "TokenNet\TokenController.h"

AP0802::AP0802()
{
	LedPins.Add(PE5);
	LedPins.Add(PE4);
	LedPins.Add(PD0);
	
	ButtonPins.Add(PE13);
	ButtonPins.Add(PE14);

	Host	= nullptr;
	HostAP	= nullptr;
	Client	= nullptr;

	Data	= nullptr;
	Size	= 0;
}

void AP0802::Init(ushort code, cstring name, COM message)
{
	auto& sys	= (TSys&)Sys;
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
	Config::Current	= &Config::CreateFlash();
}

void* AP0802::InitData(void* data, int size)
{
	// 启动信息
	auto hot	= &HotConfig::Current();
	hot->Times++;

	data = hot->Next();
	if (hot->Times == 1)
	{
		Buffer ds(data, size);
		ds.Clear();
		ds[0] = size;
	}

	Data	= data;
	Size	= size;

	return data;
}

void AP0802::InitLeds()
{
	for(int i=0; i<LedPins.Count(); i++)
	{
		auto port	= new OutputPort(LedPins[i]);
		port->Open();
		Leds.Add(port);
	}
}

void AP0802::InitButtons()
{
	for(int i=0; i<ButtonPins.Count(); i++)
	{
		auto port	= new InputPort(ButtonPins[i]);
		port->Mode	= InputPort::Both;
		port->Invert	= true;
		//port->Register(OnPress, (void*)i);
		port->Open();
		Buttons.Add(port);
	}
}

ISocketHost* AP0802::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto host	= new W5500(Spi2, PE1, PD13);
	host->NetReady.Bind(&AP0802::OpenClient, this);

	return host;
}

/*static void SetWiFiTask(void* param)
{
	auto bsp	= (AP0802*)param;
	auto client	= bsp->Client;
	auto esp	= (Esp8266*)bsp->Host;

	client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
}*/

ISocketHost* AP0802::Create8266(bool apOnly)
{
	auto host	= new Esp8266(COM4, PE2, PD3);
	//host->SetLed(WirelessLed);

	// APOnly且不是AP模式时，强制AP模式
	if(apOnly && !host->IsAP()) host->WorkMode	= SocketMode::AP;

	// 绑定委托，避免5500没有连接时导致没有启动客户端
	host->NetReady.Bind(&AP0802::OpenClient, this);

	//Sys.AddTask(SetWiFiTask, this, 0, -1, "SetWiFi");
	Client->Register("SetWiFi", &Esp8266::SetWiFi, host);

	host->OpenAsync();

	return host;
}

/******************************** Token ********************************/

void AP0802::InitClient()
{
	if(Client) return;

	auto tk = TokenConfig::Current;

	// 创建客户端
	auto client		= new TokenClient();
	client->Cfg		= tk;
	
	// 需要使用本地连接
	//client->UseLocal();

	Client = client;
	Client->MaxNotActive = 480000;

	if(Data && Size > 0)
	{
		auto& ds	= Client->Store;
		ds.Data.Set(Data, Size);
	}

	// 如果若干分钟后仍然没有打开令牌客户端，则重启系统
	Sys.AddTask(
		[](void* p){
			auto& client	= *(TokenClient*)p;
			if(!client.Opened)
			{
				debug_printf("联网超时，准备重启系统！\r\n\r\n");
				Sys.Reset();
			}
		},
		client, 8 * 60 * 1000, -1, "联网检查");
}

void AP0802::Register(int index, IDataPort& dp)
{
	if(!Client) return;

	auto& ds	= Client->Store;
	ds.Register(index, dp);
}

void AP0802::OpenClient(ISocketHost& host)
{
	assert(Client, "Client");

	debug_printf("\r\n OpenClient \r\n");

	// 网络就绪后，打开指示灯
	auto net	= dynamic_cast<W5500*>(&host);
	if(net && !net->Led) net->SetLed(*Leds[0]);

	auto esp	= dynamic_cast<Esp8266*>(&host);
	if(esp && !esp->Led) esp->SetLed(*Leds[1]);

	auto tk = TokenConfig::Current;
	NetUri uri(NetType::Udp, IPAddress::Broadcast(), 3355);

	// 避免重复打开
	if(!Client->Opened && Host)
	{
		AddControl(*Host, tk->Uri(), 0);
		AddControl(*Host, uri, tk->Port);

		Client->Open();
	}

	if(HostAP && esp)
	{
		auto ctrl	= AddControl(*HostAP, uri, tk->Port);

		// 如果没有主机，这里打开令牌客户端，为组网做准备
		if(!Host) Client->Open();

		// 假如来迟了，客户端已经打开，那么自己挂载事件
		if(Client->Opened && Client->Master)
		{
			ctrl->Received	= Client->Master->Received;
			ctrl->Open();
		}
	}
}

TokenController* AP0802::AddControl(ISocketHost& host, const NetUri& uri, ushort localPort)
{
	// 创建连接服务器的Socket
	auto socket	= host.CreateRemote(uri);

	// 创建连接服务器的控制器
	auto ctrl	= new TokenController();
	//ctrl->Port = dynamic_cast<ITransport*>(socket);
	ctrl->Socket	= socket;

	// 创建客户端
	auto client	= Client;
	if(localPort == 0)
		client->Master	= ctrl;
	else
	{
		socket->Local.Port	= localPort;
		ctrl->ShowRemote	= true;
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
	auto& bsp	= *(AP0802*)param;

	// 检查是否连接网线
	auto host	= (W5500*)bsp.Create5500();
	// 软路由的DHCP要求很严格，必须先把自己IP设为0
	host->IP	= IPAddress::Any();
	if(host->Open())
	{
		host->EnableDNS();
		host->EnableDHCP();
		bsp.Host	= host;
	}
	else
	{
		delete host;
	}

	// 没有接网线，需要完整WiFi通道
	if(!bsp.Host)
	{
		auto esp	= bsp.Create8266(false);
		if(esp)
		{
			// 未组网时，主机留空，仅保留AP主机
			bool join	= esp->SSID && *esp->SSID;
			if(join && esp->IsStation())
				bsp.Host	= esp;
			else
				bsp.HostAP	= esp;
		}
	}
	// 接了网线，同时需要AP
	else if(host->IsAP())
	{
		// 如果Host已存在，则8266仅作为AP
		auto esp	= bsp.Create8266(true);
		if(esp) bsp.HostAP	= esp;
	}

	// 打开DHCP，完成时会打开客户端
	//if(bsp.Host) bsp.Host->EnableDHCP();
}

void AP0802::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
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

ITransport* AP0802::Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
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

void AP0802::Restore()
{
	Config::Current->RemoveAll();

	Sys.Reset();
}

void AP0802::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	if (port->PressTime >= 5000)
		Restore();
	else if (port->PressTime >= 1000)
		Sys.Reset();
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
