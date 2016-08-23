#include "AP0801.h"

#include "Task.h"

#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

#include "TokenNet\TokenController.h"

AP0801* AP0801::Current = nullptr;

AP0801::AP0801()
{
	LedPins.Add(PD8);
	LedPins.Add(PE15);
	ButtonPins.Add(PE13);
	ButtonPins.Add(PE14);

	Host	= nullptr;
	HostAP	= nullptr;
	Client	= nullptr;
	ProxyFac = nullptr;
	AlarmObj = nullptr;

	Data	= nullptr;
	Size	= 0;
	// 一众标识初始化
	NetMaster  = false;
	NetBra	   = false;
	EspMaster  = false;
	EspBra		= false;
	Current		= this;
}

void AP0801::Init(ushort code, cstring name, COM message)
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

void* AP0801::InitData(void* data, int size)
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

void AP0801::InitLeds()
{
	for(int i=0; i<LedPins.Count(); i++)
	{
		auto port	= new OutputPort(LedPins[i]);
		port->Open();
		Leds.Add(port);
	}
}

void AP0801::InitButtons()
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

ISocketHost* AP0801::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto host	= new W5500(Spi2, PE1, PD13);
	host->NetReady.Bind(&AP0801::OpenClient, this);

	return host;
}

/*static void SetWiFiTask(void* param)
{
	auto bsp	= (AP0801*)param;
	auto client	= bsp->Client;
	auto esp	= (Esp8266*)bsp->Host;

	client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
}*/

ISocketHost* AP0801::Create8266(bool apOnly)
{
	auto host	= new Esp8266(COM4, PE2, PD3);

	// 初次需要指定模式 否则为 Wire
	bool join = host->SSID && *host->SSID;
	// host->Mode = SocketMode::Station;
	// if (!join) host->Mode = SocketMode::AP;
	if (!join)
	{
		*host->SSID = "WsLink";
		host->Mode = SocketMode::STA_AP;
		host->WorkMode = SocketMode::STA_AP;
	}

	// 绑定委托，避免5500没有连接时导致没有启动客户端
	host->NetReady.Bind(&AP0801::OpenClient, this);

	//Sys.AddTask(SetWiFiTask, this, 0, -1, "SetWiFi");
	Client->Register("SetWiFi", &Esp8266::SetWiFi, host);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, host);

	host->OpenAsync();

	return host;
}

/******************************** Token ********************************/

void AP0801::InitClient()
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

	// 重启
	Client->Register("Gateway/Restart", &TokenClient::InvokeRestStart, Client);
	// 重置
	Client->Register("Gateway/Reset", &TokenClient::InvokeRestBoot, Client);

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

void AP0801::Register(int index, IDataPort& dp)
{
	if(!Client) return;

	auto& ds	= Client->Store;
	ds.Register(index, dp);
}

void AP0801::OpenClient(ISocketHost& host)
{
	assert(Client, "Client");
	// debug_printf("\r\n OpenClient Flag :0x%08X\r\n", Flag);

	// 网络就绪后，打开指示灯
	auto net	= dynamic_cast<W5500*>(&host);
	if(net && !net->Led) net->SetLed(*Leds[0]);

	auto esp	= dynamic_cast<Esp8266*>(&host);
	if(esp && !esp->Led) esp->SetLed(*Leds[1]);

	auto tk = TokenConfig::Current;
	NetUri uri(NetType::Udp, IPAddress::Broadcast(), 3355);

	// 避免重复打开
	if(Host)
	{
		if (Host == esp)							// 8266 作为Host的时候  使用 Master 和广播端口两个    HostAP 为空
		{
			if (esp->Joined && !EspMaster)
			{
				AddControl(*Host, tk->Uri(), 0);	// 如果 Host 是 ESP8266 则要求 JoinAP 完成才能添加主控制器
				EspMaster = true;
			}
			if (!EspBra)
			{
				AddControl(*Host, uri, tk->Port);
				EspBra = true;
			}
		}

		if (Host == net)							// w5500 作为Host的时候    使用Master和广播两个端口     HostAP 开启AP时非空 打开其内网端口
		{
			if (!NetMaster)
			{
				AddControl(*Host, tk->Uri(), 0);					// 如果 Host 是 W5500 打开了就直接允许添加Master
				NetMaster = true;
			}
			if (!NetBra)
			{
				AddControl(*Host, uri, tk->Port);
				NetBra = true;
			}
		}

		if (HostAP && HostAP == esp)				// 只使用esp的时候HostAp为空
		{
			if (!EspBra)
			{
				AddControl(*Host, uri, tk->Port);
				EspBra = true;
			}
		}

		if (!Client->Opened)
		{
			Client->Open();
			if(ProxyFac)ProxyFac->AutoStart();
			if(AlarmObj)AlarmObj->Start();
		}
		else
			Client->AttachControls();

		// if (esp && Host == esp && esp->Joined)AddControl(*Host, tk->Uri(), 0);	// 如果 Host 是 ESP8266 则要求 JoinAP 完成才能添加主控制器
		// if (net && Host == net)AddControl(*Host, tk->Uri(), 0);					// 如果 Host 是 W5500 打开了就直接允许添加Master
		// AddControl(*Host, uri, tk->Port);
		// Client->Open();
	}

	// if(HostAP && esp)
	// {
	// 	debug_printf("4");
	// 	auto ctrl	= AddControl(*HostAP, uri, tk->Port);
	// 
	// 	// 如果没有主机，这里打开令牌客户端，为组网做准备
	// 	if(!Host) 
	// 		Client->Open();
	// 	else 
	// 		Client->AttachControls();
	// 
	// 	// 假如来迟了，客户端已经打开，那么自己挂载事件
	// 	if(Client->Opened && Client->Master)
	// 	{
	// 		ctrl->Received	= Client->Master->Received;
	// 		ctrl->Open();
	// 	}
	// }
}

TokenController* AP0801::AddControl(ISocketHost& host, const NetUri& uri, ushort localPort)
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
	auto& bsp	= *(AP0801*)param;

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

void AlarmWrite(byte type, Buffer& bs)
{
	// if (!type)return;
	Stream ms(bs);

	auto start = ms.ReadByte();
	Buffer data(bs.GetBuffer() + 1, bs.Length() - 1);
	
	auto client = AP0801::Current->Client;
	client->Store.Write(start, bs);
}

void AlarmReport(byte type, Buffer&bs)
{
	Stream ms(bs);
	auto start = ms.ReadByte();
	auto size = ms.ReadByte();
	auto client = AP0801::Current->Client;

	client->ReportAsync(start, size);
}

void AP0801::InitAlarm()
{
	if (!Client)return;

	if(!AlarmObj)AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::AlarmSet, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::AlarmGet, AlarmObj);

	AlarmObj->Register(5, AlarmWrite);
	AlarmObj->Register(6, AlarmWrite);
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

void AP0801::Restore()
{
	Config::Current->RemoveAll();

	Sys.Reset();
}

void AP0801::OnLongPress(InputPort* port, bool down)
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
