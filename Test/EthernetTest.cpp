#include "Sys.h"
#include "Enc28j60.h"
#include "SerialPort.h"
#include "TinyIP\TinyIP.h"
#include "TinyIP\Arp.h"
#include "TinyIP\Icmp.h"
#include "TinyIP\Tcp.h"
#include "TinyIP\Udp.h"
#include "TinyIP\Dhcp.h"
#include "conf.h"

TinyIP* tip;

bool OnPing(IcmpSocket* socket, ICMP_HEADER* icmp, byte* buf, uint len)
{
    debug_printf("Ping::From ");
    TinyIP::ShowIP(socket->Tip->RemoteIP);
    debug_printf(" with Payload=%d\r\n", len);

    return true;
}

bool OnUdpReceived(UdpSocket* socket, UDP_HEADER* udp, byte* buf, uint len)
{
	if(	socket->LocalPort == 137 || 
		socket->LocalPort == 1900 || 
		socket->LocalPort == 67 || 
		socket->LocalPort == 68)
		return false;

    debug_printf("Udp::On %d From ", socket->LocalPort);
    TinyIP::ShowIP(socket->RemoteIP);
    debug_printf(":%d with Payload=%d  ", socket->RemotePort, len);
    Sys.ShowString(buf, len);
    debug_printf(" \r\n");

    return socket->LocalPort == 888;
}

bool OnTcpAccepted(TcpSocket* socket, TCP_HEADER* tcp, byte* buf, uint len)
{
	if(tcp->Flags & TCP_FLAGS_ACK)
		debug_printf("Tcp::Accepted2 On %d From ", socket->LocalPort);
	else
		debug_printf("Tcp::Accepted On %d From ", socket->LocalPort);
    TinyIP::ShowIP(socket->RemoteIP);
    debug_printf(":%d with Payload=%d\r\n", socket->RemotePort, len);

    return true;
}

bool OnTcpDisconnected(TcpSocket* socket, TCP_HEADER* tcp, byte* buf, uint len)
{
    debug_printf("Tcp::Disconnected From ");
    TinyIP::ShowIP(socket->RemoteIP);
    debug_printf(":%d with Payload=%d\r\n", socket->RemotePort, len);

    return true;
}

bool OnTcpReceived(TcpSocket* socket, TCP_HEADER* tcp, byte* buf, uint len)
{
    debug_printf("Tcp::Received From ");
    TinyIP::ShowIP(socket->RemoteIP);
    debug_printf(":%d with Payload=%d  ", socket->RemotePort, len);
    Sys.ShowString(buf, len);
    debug_printf(" \r\n");

    return true;
}

bool HttpReceived(TcpSocket* socket, TCP_HEADER* tcp, byte* buf, uint len)
{
    debug_printf("Http:Received From ");
    TinyIP::ShowIP(socket->RemoteIP);
    debug_printf(":%d with Payload=%d  ", socket->RemotePort, len);
    Sys.ShowString(buf, len);
    debug_printf(" \r\n");

    return false;
}

void HttpSend(void* param)
{
	TinyIP* tip = (TinyIP*)param;
    TcpSocket tcp(tip);
	tcp.Port = 777;
    tcp.OnReceived = HttpReceived;

	// 连接
	byte ip[] = {192, 168, 0, 84};
	tcp.Connect(*(uint*)ip, 80);
	debug_printf("\r\n");

	// 发送数据
	byte str[] = "GET / HTTP/1.1\r\nHost: 192.168.0.84\r\n\r\n\r\n";
	tcp.Send(str, ArrayLength(str));

	// 等待接收
	Sys.Sleep(3000);

	// 断开
	tcp.Close();
}

void UdpSend(void* param)
{
	TinyIP* tip = (TinyIP*)param;
    UdpSocket udp(tip);
	udp.Port = 777;

	// 连接
	byte ip[] = {192, 168, 0, 84};
	udp.RemoteIP = *(uint*)ip;

	// 发送数据
	byte str[] = "GET / HTTP/1.1\r\nHost: 192.168.0.84\r\n\r\n";
	udp.Send(str, ArrayLength(str));
}

void OnDhcpStop(void* sender, void* param)
{
	Dhcp* dhcp = (Dhcp*)sender;
	if(!dhcp->Result) return;

	IcmpSocket* icmp = (IcmpSocket*)tip->Sockets.FindByType(IP_ICMP);

    // 测试Ping网关
    for(int i=0; i<4; i++)
    {
        /*debug_printf("Ping ");
        tip->ShowIP(tip->Gateway);
        debug_printf("\r\n");*/
        if(icmp->Ping(tip->Gateway))
            debug_printf("Ping Success\r\n");
        else
            debug_printf("Ping Fail\r\n");
    }
	byte ip[] = {192, 168, 0, 84};
	icmp->Ping(*(uint*)ip);

	debug_printf("\r\n Http收发测试 ");
	Sys.AddTask(HttpSend, tip, 0, -1);
}

void TestEthernet()
{
    debug_printf("\r\n");
    debug_printf("TestEthernet Start......\r\n");

    Spi* spi = new Spi(SPI2);
    Enc28j60* enc = new Enc28j60(spi);

    //tip = new TinyIP(enc, myip, mymac);
    tip = new TinyIP(enc);
    enc->Init((byte*)&tip->Mac);

	// 如果不是为了Dhcp，这里可以不用Open，Init里面会Open
    if(!tip->Open()) return;

    IcmpSocket* icmp = new IcmpSocket(tip);
    icmp->OnPing = OnPing;

    UdpSocket* udp = new UdpSocket(tip);
    udp->OnReceived = OnUdpReceived;
	udp->Open();

    TcpSocket* tcp = new TcpSocket(tip);
    tcp->OnAccepted = OnTcpAccepted;
    tcp->OnDisconnected = OnTcpDisconnected;
    tcp->OnReceived = OnTcpReceived;
	tcp->Open();

	Dhcp* dhcp = new Dhcp(tip);
	dhcp->OnStop = OnDhcpStop;
    dhcp->Start();

    debug_printf("TestEthernet Finish!\r\n");
}
