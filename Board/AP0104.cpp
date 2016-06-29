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

static FlushPort* CreateFlushPort(OutputPort* led)
{
	auto fp	= new FlushPort();
	fp->Port	= led;
	fp->Start();

	return fp;
}

AP0104::AP0104()
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

void AP0104::Setup(ushort code, cstring name, COM message, int baudRate)
{
	auto& sys = (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

	// 初始化系统
	//Sys.Clock = 48000000;
	sys.Init();
#if DEBUG
	sys.MessagePort = message; // 指定printf输出的串口
	Sys.ShowInfo();
#endif

#if DEBUG
	// 设置一定量初始任务，减少堆分配
	static Task ts[0x10];
	Task::Scheduler()->Set(ts, ArrayLength(ts));
#endif

#if DEBUG
	// 打开串口输入便于调试数据操作，但是会影响性能
	if (baudRate > 0)
	{
		auto sp = SerialPort::GetMessagePort();
		if (baudRate != 1024000)
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
	Config::Current = &Config::CreateFlash();
}

// 网络就绪
void OnNetReady(AP0104& ap, ISocketHost& host)
{
	if (ap.Client) ap.Client->Open();
}

ISocketHost* AP0104::Open5500()
{
	IDataPort* led = nullptr;
	if(EthernetLed) led	= CreateFlushPort(EthernetLed);

	auto host	= (W5500*)Create5500(Spi1, PE7, PB2, led);
	host->NetReady.Bind(OnNetReady, this);
	if(host->Open()) return host;

	delete host;

	return nullptr;
}

ISocketHost* AP0104::Create5500(SPI spi, Pin irq, Pin rst, IDataPort* led)
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
	auto ap	= (AP0104*)param;
	auto client	= ap->Client;
	auto esp	= (Esp8266*)ap->Host;

	client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
}

ISocketHost* AP0104::Open8266(bool apOnly)
{
	auto host	= (Esp8266*)Create8266(COM4, P0, P0);

	if(WirelessLed) host->Led	= CreateFlushPort(WirelessLed);

	if(apOnly) host->WorkMode	= SocketMode::AP;

	Sys.AddTask(SetWiFiTask, this, 0, -1, "SetWiFi");
	host->NetReady.Bind(OnNetReady, this);

	host->OpenAsync();

	return host;
}



ISocketHost* AP0104::Create8266(COM idx, Pin power, Pin rst)
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

void AP0104::CreateClient()
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

void AP0104::OpenClient()
{
	debug_printf("\r\n OpenClient \r\n");
	assert(Host, "Host");

	auto tk = TokenConfig::Current;
	AddControl(*Host, *tk);

	TokenConfig cfg;
	cfg.Protocol	= NetType::Udp;
	cfg.ServerIP	= IPAddress::Broadcast().Value;
	cfg.ServerPort	= 3355;
	AddControl(*Host, cfg);

	if(HostAP) AddControl(*HostAP, cfg);
}

void AP0104::AddControl(ISocketHost& host, TokenConfig& cfg)
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
