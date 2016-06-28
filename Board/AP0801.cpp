#include "AP0801.h"

#include "Task.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

#include "Net\Dhcp.h"
#include "Net\DNS.h"

#include "TokenNet\TokenController.h"

#include "App\FlushPort.h"

static FlushPort* CreateFlushPort(OutputPort* led)
{
	auto fp	= new FlushPort();
	fp->Port	= led;
	fp->Start();

	return fp;
}

AP0801::AP0801()
{
	EthernetLed	= nullptr;
	WirelessLed	= nullptr;

	Host	= nullptr;
	HostAP	= nullptr;
	Client	= nullptr;
}

#if DEBUG
static uint OnSerial(ITransport* transport, Buffer& bs, void* param, void* param2)
{
	debug_printf("OnSerial len=%d \t", bs.Length());
	bs.Show(true);

	return 0;
}
#endif

void AP0801::Setup(ushort code, cstring name, COM message, int baudRate)
{
	auto& sys	= (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

    // 初始化系统
    sys.Init();
#if DEBUG
    sys.MessagePort = message; // 指定printf输出的串口
    Sys.ShowInfo();
#endif

#if DEBUG
	// 打开串口输入便于调试数据操作，但是会影响性能
	if(baudRate > 0)
	{
		auto sp = SerialPort::GetMessagePort();
		if(baudRate != 1024000)
		{
			sp->Close();
			sp->SetBaudRate(baudRate);
		}
		sp->Register(OnSerial);
	}

	//WatchDog::Start(20000);
#else
	WatchDog::Start();
#endif

	// Flash最后一块作为配置区
	Config::Current	= &Config::CreateFlash();
}

// 网络就绪
void OnNetReady(AP0801& ap, ISocketHost& host)
{
	if (ap.Client) ap.Client->Open();
}

ISocketHost* AP0801::Open5500()
{
	IDataPort* led = nullptr;
	if(EthernetLed) led	= CreateFlushPort(EthernetLed);

	auto host	= (W5500*)Create5500(Spi2, PE1, PD13, led);
	host->NetReady	= Delegate<ISocketHost&>(OnNetReady, this);
	if(host->Open()) return host;

	delete host;

	return nullptr;
}

ISocketHost* AP0801::Create5500(SPI spi, Pin irq, Pin rst, IDataPort* led)
{
	debug_printf("\r\nW5500::Create \r\n");

	auto spi_	= new Spi(spi, 36000000);

	auto net	= new W5500();
	net->LoadConfig();
	net->Init(spi_, irq, rst);

	return net;
}

static void SetWiFiTask(void* param)
{
	auto ap	= (AP0801*)param;
	auto client	= ap->Client;
	auto esp	= (Esp8266*)ap->Host;

	client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
}

ISocketHost* AP0801::Open8266(bool apOnly)
{
	auto host	= (Esp8266*)Create8266(COM4, PE2, PD3);

	if(WirelessLed) host->Led	= CreateFlushPort(WirelessLed);

	if(apOnly) host->WorkMode	= SocketMode::AP;

	Sys.AddTask(SetWiFiTask, this, 0, -1, "SetWiFi");
	host->NetReady	= Delegate<ISocketHost&>(OnNetReady, this);

	host->OpenAsync();

	return host;
}

ISocketHost* AP0801::Create8266(COM idx, Pin power, Pin rst)
{
	debug_printf("\r\nEsp8266::Create \r\n");

	auto srp	= new SerialPort(idx, 115200);
	srp->Tx.SetCapacity(0x100);
	srp->Rx.SetCapacity(0x100);

	auto net	= new Esp8266(srp, power, rst);
	net->InitConfig();
	net->LoadConfig();

	// 配置模式作为工作模式
	net->WorkMode	= net->Mode;

	return net;
}

/******************************** Token ********************************/

void AP0801::CreateClient()
{
	if(Client) return;

	auto tk = TokenConfig::Current;

	// 创建客户端
	auto client		= new TokenClient();
	//client->Control	= ctrl;
	//client->Local	= ctrl;
	client->Cfg		= tk;

	Client	= client;
}

void AP0801::OpenClient()
{
	debug_printf("\r\n OpenClient \r\n");
	assert(Host, "Host");

	auto tk = TokenConfig::Current;
	AddControl(*Host, *tk);

	TokenConfig cfg;
	cfg.Protocol	= ProtocolType::Udp;
	cfg.ServerIP	= IPAddress::Broadcast().Value;
	cfg.ServerPort	= 3355;
	AddControl(*Host, cfg);

	if(HostAP) AddControl(*HostAP, cfg);
}

void AP0801::AddControl(ISocketHost& host, TokenConfig& cfg)
{
	// 创建连接服务器的Socket
	auto socket	= host.CreateSocket(cfg.Protocol);
	socket->Remote.Port		= cfg.ServerPort;
	socket->Remote.Address	= IPAddress(cfg.ServerIP);
	socket->Server	= cfg.Server();

	// 创建连接服务器的控制器
	auto ctrl		= new TokenController();
	//ctrl->Port = dynamic_cast<ITransport*>(socket);
	ctrl->Socket	= socket;

	// 创建客户端
	auto client		= Client;
	client->Controls.Add(ctrl);

	// 如果不是第一个，则打开远程
	if(client->Controls.Count() > 1) ctrl->ShowRemote	= true;
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
