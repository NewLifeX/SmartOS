#include "Token.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\ShunCom.h"
#include "Drivers\Enc28j60.h"

#include "TinyIP\TinyIP.h"

#include "Net\Dhcp.h"
#include "Net\DNS.h"

#include "TokenNet\Gateway.h"
#include "TokenNet\Token.h"

#include "Security\RC4.h"

#include "App\FlushPort.h"

#define ShunCom_Master 0

static void StartGateway(void* param);

static void OnDhcpStop(void* sender, void* param)
{
	auto dhcp = (Dhcp*)sender;

	if(dhcp->Times <= 1) Sys.AddTask(StartGateway, &dhcp->Host, 0, -1, "启动网关");
}

ISocketHost* Token::CreateW5500(SPI spi_, Pin irq, Pin rst, Pin power, IDataPort* led)
{
	debug_printf("\r\nW5500::Create \r\n");

	auto spi	= new Spi(spi_, 36000000);

	debug_printf("\tPower: ");
	auto pwr	= new OutputPort(power, true);
	*pwr = true;

	auto net	= new W5500();
	net->LoadConfig();
	net->Init(spi, irq, rst);
	net->Led	= led;

	return net;
}

ISocketHost* Token::Create2860(SPI spi_, Pin irq, Pin rst)
{
	debug_printf("\r\nENC2860::Create \r\n");

	auto spi	= new Spi(spi_, 9000000);
	auto _enc	= new Enc28j60();
	_enc->Init(spi, irq, rst);

	auto _tip	= new TinyIP();
	_tip->Init(_enc);

	_enc->Mac = _tip->Mac;
	if(!_tip->Open()) return NULL;

	Sys.Sleep(500);
	if(!_enc->Linked()) debug_printf("未连接网线！\r\n");

	// 通过DHCP获取IP期间，关闭Arp响应
	//_tip.Arp->Enable = false;

	return _tip;
}

TokenClient* Token::CreateClient(ISocketHost* host)
{
	debug_printf("\r\nCreateClient \r\n");

	// 打开DHCP
	//static UdpClient udp(net);
	static Dhcp	dhcp(*host);
	dhcp.OnStop	= OnDhcpStop;
	dhcp.Start();

	auto tk = TokenConfig::Current;
	auto socket	= host->CreateSocket(tk->Protocol);
	socket->Remote.Port		= tk->ServerPort;
	socket->Remote.Address	= IPAddress(tk->ServerIP);

	static TokenController ctrl;
	ctrl.Port = dynamic_cast<ITransport*>(socket);

	static TokenClient client;
	client.Control	= &ctrl;
	//client->Local	= ctrl;

	// 如果是TCP，需要再建立一个本地UDP
	//if(tk->Protocol == ProtocolType::Tcp)
	{
		socket	= host->CreateSocket(ProtocolType::Udp);
		socket->Remote.Port		= 3355;	// 广播端口。其实用哪一个都不重要，因为不会主动广播
		socket->Remote.Address	= IPAddress::Broadcast();
		socket->Local.Port	= tk->Port;
		auto token2		= new TokenController();
		token2->Port	= dynamic_cast<ITransport*>(socket);
		client.Local	= token2;
	}

	return &client;
}

TinyServer* Token::CreateServer(ITransport* port)
{
	debug_printf("\r\n CreateServer \r\n");

	static TinyController ctrl;
	ctrl.Port = port;
	ctrl.QueueLength = 64;
	ctrl.ApplyConfig();

	// 新配置需要保存一下
	auto tc = TinyConfig::Current;
	if(tc && tc->New) tc->Save();

	static TinyServer server(&ctrl);
	server.Cfg	= tc;

	return &server;
}

uint OnSerial(ITransport* transport, Buffer& bs, void* param, void* param2)
{
	debug_printf("OnSerial len=%d \t", bs.Length());
	bs.Show(true);

	//TokenClient* client = TokenClient::Current;
	//if(client) client->Store.Write(1, bs);

	return 0;
}

void Token::Setup(ushort code, const char* name, COM message, int baudRate)
{
	auto& sys	= (TSys&)Sys;
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

void Fix2401(void* param)
{
	auto& bs	= *(Buffer*)param;
	// 微网指令特殊处理长度
	uint rs	= bs.Length();
	if(rs >= 8)
	{
		rs = bs[5] + 8;
		if(rs < bs.Length()) bs.SetLength(rs);
	}
}

ITransport* Token::Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
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
	nrf.Led		= led;

	nrf.Master	= true;

	return &nrf;
}

ITransport* Token::CreateShunCom(COM index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg, IDataPort* led)
{
	debug_printf("\r\n CreateShunCom \r\n");

	auto tc	= TinyConfig::Create();
	if(tc->Channel == 0)
	{
		tc->Channel	= 0x0F;
		tc->Speed	= 250;
	}
	if(tc->Interval == 0)
	{
		tc->Interval= 800;
		tc->Timeout	= 1600;
	}

	static SerialPort sp(index, baudRate);
	static ShunCom zb;
	zb.Power.Set(power);
	zb.Sleep.Init(slp, true);
	zb.Config.Init(cfg, true);
	zb.Init(&sp, rst);
	zb.Led = led;

#if ShunCom_Master
	zb.AddrLength = 2;

	if(tc->Channel != 0x0F)
	{
		if(zb.EnterConfig())
		{
			zb.ShowConfig();
			zb.SetDevice(0x00);
			//zb.SetPanID(0x4444);
			//zb.EraConfig();
			tc->Channel = 0x0F;
			tc->Save();
			zb.SetSend(0x01);
			zb.PrintSrc(true);
			zb.ExitConfig();
		}
	}
#endif
	return &zb;
}

void StartGateway(void* param)
{
	ISocket* socket	= NULL;
	auto gw	= Gateway::Current;
	if(gw) socket = dynamic_cast<ISocket*>(gw->Client->Control->Port);

	auto tk = TokenConfig::Current;

	if(tk && tk->Server.Length() > 0)
	{
		// 根据DNS获取云端IP地址
		auto ip	= DNS::Query(*(ISocketHost*)param, tk->Server);
		if(ip == IPAddress::Any())
		{
			debug_printf("DNS::Query %s 失败！\r\n", tk->Server.GetBuffer());
			return;
		}
		debug_printf("服务器地址 %s %s:%d \r\n", tk->Server.GetBuffer(), ip.ToString().GetBuffer(), tk->ServerPort);

		if(socket) socket->Remote.Address = ip;
		tk->ServerIP = ip.Value;
		tk->Save();
	}

	// 此时启动网关服务
	if(gw)
	{
		auto& ep = gw->Client->Hello.EndPoint;
		if(socket) ep.Address = socket->Host->IP;

		if(!gw->Running)
		{
			gw->Start();

			debug_printf("\r\n");
			// 启动时首先进入学习模式
			//gw->SetMode(30);
		}
	}
}

void Token::SetPower(ITransport* port)
{
	auto pwr	= dynamic_cast<Power*>(port);
	if(pwr) pwr->SetPower();
}
