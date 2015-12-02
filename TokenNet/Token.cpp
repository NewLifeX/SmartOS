#include "Token.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\ShunCom.h"

#include "Net\Dhcp.h"
#include "Net\DNS.h"

#include "TokenNet\Gateway.h"
#include "TokenNet\Token.h"

#include "Security\RC4.h"

#include "App\FlushPort.h"

void StartGateway(void* param);

void OnDhcpStop5500(void* sender, void* param)
{
	auto dhcp = (Dhcp*)sender;
	if(!dhcp->Result)
	{
		// 失败后重新开始DHCP，等待网络连接
		dhcp->Start();

		return;
	}

	// 获取IP成功，重新设置参数
	auto net = (W5500*)dhcp->Host;
	net->Config();
	net->ShowInfo();
	net->SaveConfig();

	if(dhcp->Times <= 1) Sys.AddTask(StartGateway, net, 0, -1, "启动网关");
}

ISocketHost* Token::CreateW5500(SPI_TypeDef* spi_, Pin irq, Pin rst, Pin power, IDataPort* led)
{
	debug_printf("\r\nW5500::Create \r\n");

	auto spi = new Spi(spi_, 36000000);

	auto pwr = new OutputPort(power, true);
	*pwr = true;

	//TokenConfig* tk = TokenConfig::Current;

	auto net = new W5500();
	net->LoadConfig();
	net->Init(spi, irq, rst);
	net->Led = led;

	// 打开DHCP
	auto udp	= new UdpClient(net);
	auto dhcp	= new Dhcp(udp);
	dhcp->OnStop	= OnDhcpStop5500;
	dhcp->Start();

	return net;
}

ISocket* CreateW5500UDP(ISocketHost* host, TokenConfig* tk)
{
	auto udp	= new UdpClient((W5500*)host);
	//udp->Local.Port	= tk->Port;
	udp->Remote.Port	= tk->ServerPort;
	udp->Remote.Address	= IPAddress(tk->ServerIP);

	return udp;
}

ISocket* CreateW5500TCP(ISocketHost* host, TokenConfig* tk)
{
	auto tcp	= new TcpClient((W5500*)host);
	//tcp->Local.Port	= tk->Port;
	tcp->Remote.Port	= tk->ServerPort;
	tcp->Remote.Address	= IPAddress(tk->ServerIP);

	return tcp;
}

TokenClient* Token::CreateClient(ISocketHost* host)
{
	debug_printf("\r\nCreateClient \r\n");

	auto token	= new TokenController();

	auto tk = TokenConfig::Current;
	ISocket* socket	= NULL;
	if(tk->Protocol == 0)
		socket = CreateW5500UDP(host, tk);
	else
		socket = CreateW5500TCP(host, tk);
	token->Port = dynamic_cast<ITransport*>(socket);

	auto client	= new TokenClient();
	client->Control	= token;
	//client->Local	= token;

	// 如果是TCP，需要再建立一个本地UDP
	//if(tk->Protocol == 1)
	{
		TokenConfig tc;
		//tc.Port			= tk->Port;
		tc.ServerIP		= IPAddress::Broadcast().Value;
		tc.ServerPort	= 3355;	// 广播端口。其实用哪一个都不重要，因为不会主动广播

		socket	= CreateW5500UDP(host, &tc);
		socket->Local.Port	= tk->Port;
		token	= new TokenController();
		token->Port = dynamic_cast<ITransport*>(socket);
		client->Local	= token;
	}

	return client;
}

TinyServer* Token::CreateServer(ITransport* port)
{
	debug_printf("\r\nCreateServer \r\n");

	auto ctrl	= new TinyController();
	ctrl->Port = port;

	// 只有2401需要打开重发机制
	//if(strcmp(port->ToString(), "R24")) ctrl->Timeout = -1;

	auto tc = TinyConfig::Current;
	tc->Address = ctrl->Address;

	auto server	= new TinyServer(ctrl);
	server->Cfg	= tc;

	return server;
}

uint OnSerial(ITransport* transport, Array& bs, void* param, void* param2)
{
	debug_printf("OnSerial len=%d \t", bs.Length());
	bs.Show(true);

	//TokenClient* client = TokenClient::Current;
	//if(client) client->Store.Write(1, bs);

	return 0;
}

void Token::Setup(ushort code, const char* name, COM_Def message, int baudRate)
{
	Sys.Code = code;
	Sys.Name = (char*)name;

    // 初始化系统
    //Sys.Clock = 48000000;
    Sys.Init();
#if DEBUG
    Sys.MessagePort = message; // 指定printf输出的串口
    Sys.ShowInfo();
#endif

#if DEBUG
	// 打开串口输入便于调试数据操作，但是会影响性能
	auto sp = SerialPort::GetMessagePort();
	if(baudRate != 1024000)
	{
		sp->Close();
		sp->SetBaudRate(baudRate);
	}
	sp->Register(OnSerial);

	//WatchDog::Start(20000);
#else
	WatchDog::Start();
#endif

	// Flash最后一块作为配置区
	Config::Current	= &Config::CreateFlash();
}

ITransport* Token::Create2401(SPI_TypeDef* spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
{
	auto spi = new Spi(spi_, 10000000, true);
	auto nrf = new NRF24L01();
	nrf->Init(spi, ce, irq, power);
	nrf->Power.Invert = powerInvert;

	nrf->AutoAnswer		= false;
	nrf->PayloadWidth	= 32;
	nrf->Channel		= TinyConfig::Current->Channel;
	nrf->Speed			= TinyConfig::Current->Speed;

	nrf->Led	= led;
	//if(!nrf.Check()) debug_printf("请检查NRF24L01线路\r\n");

	return nrf;
}

uint OnZig(ITransport* port, Array& bs, void* param, void* param2)
{
	debug_printf("配置信息\r\n");
	bs.Show(true);

	return 0;
}

ITransport* Token::CreateShunCom(COM_Def index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg, IDataPort* led)
{
	auto sp = new SerialPort(index, baudRate);
	auto zb = new ShunCom();
	zb->Power.Set(power);
	zb->Sleep.Init(slp, true);
	zb->Config.Init(cfg, true);
	zb->Init(sp, rst);

	zb->Led	= led;

	return zb;
}

void StartGateway(void* param)
{
	ISocket* socket	= NULL;
	auto gw	= Gateway::Current;
	if(gw) socket = dynamic_cast<ISocket*>(gw->Client->Control->Port);

	auto tk = TokenConfig::Current;

	if(tk && tk->Server[0])
	{
		W5500* net	= (W5500*)param;
		// 根据DNS获取云端IP地址
		UdpClient udp(net);
		DNS dns(&udp);
		udp.Open();

		for(int i=0; i<10; i++)
		{
			//IPAddress ip =IPAddress(192, 168, 0, 6);
			auto ip = dns.Query(tk->Server, 2000);
			//dns.Query(tk->Server, 2000);
			ip.Show(true);

			if(ip != IPAddress::Any())
			{
				tk->ServerIP = ip.Value;

				if(socket) socket->Remote.Address = ip;

				break;
			}
		}

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
			gw->SetMode(30);
		}
	}
}

void Token::SetPower(ITransport* port)
{
	auto pwr	= dynamic_cast<Power*>(port);
	if(pwr) pwr->SetPower();
}
