#include "AP0104.h"

#include "Task.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266.h"

#include "Net\Dhcp.h"
#include "Net\DNS.h"

#include "TokenNet\TokenController.h"

#include "App\FlushPort.h"

static FlushPort* CreateFlushPort(OutputPort* led)
{
	auto fp = new FlushPort();
	fp->Port = led;
	fp->Start();

	return fp;
}

AP0104::AP0104()
{
	EthernetLed = nullptr;
	WirelessLed = nullptr;
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

ISocketHost* AP0104::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");
	IDataPort* led = nullptr;
	if(EthernetLed) led	= CreateFlushPort(EthernetLed);

	return Host	= Create5500(Spi1, PE7, PB2, led);
}

ISocketHost* AP0104::Create5500(SPI spi, Pin irq, Pin rst, IDataPort* led)
{
	debug_printf("\r\nW5500::Create \r\n");

	auto spi_	= new Spi(spi, 36000000);

	auto net	= new W5500();
	net->LoadConfig();
	net->Init(spi_, irq, rst);
	//net->EnableDNS();

	return net;
}

ISocketHost* AP0104::Create8266()
{
	debug_printf("\r\nEsp8266::Create \r\n");

	/*// 上电
	auto pwr	= new OutputPort(PE2);
	pwr->Down(1000);*/

	auto srp = new SerialPort(COM4, 115200);
	//srp->ByteTime	= 10;
	srp->Tx.SetCapacity(0x400);
	srp->Rx.SetCapacity(0x400);

	auto net = new Esp8266(srp, P0, P0);
	net->InitConfig();
	net->LoadConfig();

	//net->SSID = "yws007";
	//net->Pass = "yws52718";
	//net->NetReady = onNetReady;

	if (EthernetLed) net->Led = CreateFlushPort(EthernetLed);

	net->OpenAsync();

	Host = net;
	//net->LoadAPs();
	return net;
}

/******************************** DHCP ********************************/

static Action _DHCP_Ready = nullptr;

static void On_DHCP_Ready(void* param)
{
	if (_DHCP_Ready) _DHCP_Ready(param);
}

static void OnDhcpStop(void* sender, void* param)
{
	auto& dhcp = *(Dhcp*)sender;

	// DHCP成功，或者失败且超过最大错误次数，都要启动网关，让它以上一次配置工作
	if (dhcp.Result || dhcp.Times >= dhcp.MaxTimes)
	{
		// 防止调用栈太深，另外开任务
		if (_DHCP_Ready) Sys.AddTask(On_DHCP_Ready, &dhcp.Host, 0, -1, "网络就绪");
	}
}

void AP0104::InitDHCP(Action onNetReady)
{
	_DHCP_Ready = onNetReady;

	// 打开DHCP
	auto dhcp = new Dhcp(*Host);
	dhcp->OnStop = OnDhcpStop;
	dhcp->Start();
}

/******************************** DNS ********************************/

bool AP0104::QueryDNS(TokenConfig& tk)
{
	auto svr	= tk.Server();
	if (svr.Length() == 0) return false;

	// 根据DNS获取云端IP地址
	auto ip = DNS::Query(*Host, svr);
	if (ip == IPAddress::Any())
	{
		debug_printf("DNS::Query %s 失败！\r\n", svr.GetBuffer());
		return false;
	}
	debug_printf("服务器地址 %s %s:%d \r\n", svr.GetBuffer(), ip.ToString().GetBuffer(), tk.ServerPort);

	tk.ServerIP = ip.Value;
	tk.Save();

	return true;
}

/******************************** Token ********************************/

TokenClient* AP0104::CreateClient()
{
	debug_printf("\r\nCreateClient \r\n");

	auto tk = TokenConfig::Current;

	// 创建连接服务器的Socket
	auto socket = Host->CreateSocket(tk->Protocol);
	socket->Remote.Port = tk->ServerPort;
	socket->Remote.Address = IPAddress(tk->ServerIP);

	// 创建连接服务器的控制器
	auto ctrl = new TokenController();
	ctrl->Port = dynamic_cast<ITransport*>(socket);

	// 创建客户端
	auto client = new TokenClient();
	client->Control = ctrl;
	//client->Local	= ctrl;`
	client->Cfg = tk;

	// 如果是TCP，需要再建立一个本地UDP
	//if(tk->Protocol == ProtocolType::Tcp)
	{
		// 建立一个监听内网的UDP Socket
		socket = Host->CreateSocket(ProtocolType::Udp);
		socket->Remote.Port = 3355;	// 广播端口。其实用哪一个都不重要，因为不会主动广播
		socket->Remote.Address = IPAddress::Broadcast();
		socket->Local.Port = tk->Port;

		// 建立内网控制器
		auto token2 = new TokenController();
		token2->Port = dynamic_cast<ITransport*>(socket);
		client->Local = token2;
	}

	return client;
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

ITransport* AP0104::Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
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
