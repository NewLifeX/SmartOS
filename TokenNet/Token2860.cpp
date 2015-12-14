#include "Token.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\Enc28j60.h"
#include "Drivers\ShunCom.h"

#include "TinyIP\Icmp.h"
#include "TinyIP\Tcp.h"
#include "TinyIP\Udp.h"

#include "Net\Dhcp.h"
#include "Net\DNS.h"

#include "TokenNet\Gateway.h"
#include "TokenNet\Token.h"

#include "Security\RC4.h"

#include "App\FlushPort.h"

static void StartGateway(void* param);

static void OnDhcpStop(void* sender, void* param)
{
	auto dhcp = (Dhcp*)sender;
	if(!dhcp->Result)
	{
		// 失败后重新开始DHCP，等待网络连接
		dhcp->Start();

		return;
	}

	auto udp = (UdpSocket*)dhcp->Socket;
	auto tip = udp->Tip;

	// 通过DHCP获取IP期间，关闭Arp响应
	if(tip->Arp) tip->Arp->Enable = true;

	if(dhcp->Times <= 1) Sys.AddTask(StartGateway, tip, 0, -1, "启动网关");
}

ISocketHost* Token::Create2860(SPI_TypeDef* spi_, Pin irq, Pin rst)
{
	debug_printf("\r\nENC2860::Create \r\n");

	static Spi spi(spi_, 9000000);
	static Enc28j60 _enc;
	_enc.Init(&spi, irq, rst);

	static TinyIP _tip;

	_enc.Mac = _tip.Mac;
	if(!_tip.Open()) return NULL;
	//Sys.Sleep(40);
	if(!_enc.Linked()) debug_printf("未连接网线！\r\n");

	//!!! 非常悲催，dhcp完成的时候，会释放自己，所以这里必须动态申请内存，否则会导致堆管理混乱
	static UdpSocket udp(&_tip);
	static Dhcp	dhcp(&udp);
	dhcp.OnStop	= OnDhcpStop;
	// 通过DHCP获取IP期间，关闭Arp响应
	_tip.Arp->Enable = false;
	dhcp.Start();

	return &_tip;	
}

ISocket* CreateUDP(ISocketHost* host, TokenConfig* tk)
{
	auto udp = new UdpSocket((TinyIP*)host);
	//udp->Local.Port = tk->Port;
	udp->Remote.Port	= tk->ServerPort;
	udp->Remote.Address	= IPAddress(tk->ServerIP);

	return udp;
}

ISocket* CreateTcp(ISocketHost* host, TokenConfig* tk)
{
	auto tcp = new TcpSocket((TinyIP*)host);
	//tcp->Local.Port = tk->Port;
	tcp->Remote.Port	= tk->ServerPort;
	tcp->Remote.Address	= IPAddress(tk->ServerIP);

	return tcp;
}

TokenClient* Token::CreateClient2860(ISocketHost* host)
{
	debug_printf("\r\nCreateClient \r\n");

	static TokenController token;

	auto tk = TokenConfig::Current;
	ISocket* socket	= NULL;
	if(tk->Protocol == 0)
		socket = CreateUDP(host, tk);
	else
		socket = CreateTcp(host, tk);
	token.Port = dynamic_cast<ITransport*>(socket);

	static TokenClient client;
	client.Control	= &token;
	//client->Local	= token;

	// 如果是TCP，需要再建立一个本地UDP
	//if(tk->Protocol == 1)
	{
		TokenConfig tc;
		//tc.Port			= tk->Port;
		tc.ServerIP		= IPAddress::Broadcast().Value;
		tc.ServerPort	= 3355;	// 广播端口。其实用哪一个都不重要，因为不会主动广播

		socket	= CreateUDP(host, &tc);
		socket->Local.Port	= tk->Port;
		auto token2		= new TokenController();
		token2->Port	= dynamic_cast<ITransport*>(socket);
		client.Local	= token2;
	}

	return &client;
}

void StartGateway(void* param)
{
	ISocket* socket	= NULL;
	auto gw	= Gateway::Current;
	if(gw) socket = dynamic_cast<ISocket*>(gw->Client->Control->Port);

	auto tk = TokenConfig::Current;

	if(tk && tk->Server[0])
	{
		// 根据DNS获取云端IP地址
		UdpSocket udp((TinyIP*)param);
		DNS dns(&udp);
		udp.Open();

		for(int i=0; i<10; i++)
		{
			auto ip = dns.Query(tk->Server, 2000);
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
