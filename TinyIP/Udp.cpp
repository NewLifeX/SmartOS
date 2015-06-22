﻿#include "Udp.h"

#define NET_DEBUG DEBUG

UdpSocket::UdpSocket(TinyIP* tip) : Socket(tip)
{
	Type		= IP_UDP;
	Port		= 0;
	RemoteIP	= IPAddress::Any;
	RemotePort	= 0;
	LocalIP		= IPAddress::Any;
	LocalPort	= 0;

	// 累加端口
	static ushort g_udp_port = 1024;
	if(g_udp_port < 1024) g_udp_port = 1024;
	BindPort = g_udp_port++;
}

string UdpSocket::ToString()
{
	static char name[10];
	sprintf(name, "UDP_%d", Port);
	return name;
}

bool UdpSocket::OnOpen()
{
	if(Port != 0) BindPort = Port;

	if(Port)
		debug_printf("Udp::Open %d 过滤 %d\r\n", BindPort, Port);
	else
		debug_printf("Udp::Open %d 监听所有端口UDP数据包\r\n", BindPort);

	Enable = true;
	return Enable;
}

void UdpSocket::OnClose()
{
	debug_printf("Udp::Close %d\r\n", BindPort);
	Enable = false;
}

bool UdpSocket::Process(IP_HEADER& ip, Stream& ms)
{
	UDP_HEADER* udp = ms.Retrieve<UDP_HEADER>();
	if(!udp) return false;

	ushort port = __REV16(udp->DestPort);
	ushort remotePort = __REV16(udp->SrcPort);

	// 仅处理本连接的IP和端口
	if(Port != 0 && port != Port) return false;

	RemotePort	= remotePort;
	RemoteIP	= ip.SrcIP;
	LocalPort	= port;
	LocalIP		= ip.DestIP;

#if NET_DEBUG
	byte* data	= udp->Next();
	uint len	= ms.Remain();
	uint plen	= __REV16(udp->Length);
	assert_param(len + sizeof(UDP_HEADER) == plen);
	//Sys.ShowHex((byte*)udp, udp->Size(), '-');
	//debug_printf("\r\n");
#endif

	OnProcess(ip, *udp, ms);

	return true;
}

void UdpSocket::OnProcess(IP_HEADER& ip, UDP_HEADER& udp, Stream& ms)
{
	byte* data = ms.Current();
	//uint len = ms.Remain();
	// 计算标称的数据长度
	uint len = __REV16(udp.Length) - sizeof(UDP_HEADER);
	assert_param(len <= ms.Remain());

	// 触发ITransport接口事件
	uint len2 = OnReceive(data, len);
	// 如果有返回，说明有数据要回复出去
	if(len2) Write(data, len2);

	if(OnReceived)
	{
		// 返回值指示是否向对方发送数据包
		bool rs = OnReceived(*this, udp, ms);
		if(rs && ms.Remain() > 0) Send(udp, len, false);
	}
	else
	{
#if NET_DEBUG
		debug_printf("UDP ");
		RemoteIP.Show();
		debug_printf(":%d => ", RemotePort);
		LocalIP.Show();
		debug_printf(":%d Payload=%d udp_len=%d \r\n", LocalPort, len, __REV16(udp.Length));

		Sys.ShowHex(data, len);
		debug_printf(" \r\n");
#endif
	}
}

void UdpSocket::Send(UDP_HEADER& udp, uint len, bool checksum)
{
	uint tlen		= sizeof(UDP_HEADER) + len;
	udp.SrcPort		= __REV16(Port > 0 ? Port : LocalPort);
	udp.DestPort	= __REV16(RemotePort);
	udp.Length		= __REV16(tlen);

	// 网络序是大端
	udp.Checksum	= 0;
	if(checksum) udp.Checksum = __REV16(Tip->CheckSum(&RemoteIP, (byte*)&udp, tlen, 1));

	debug_printf("SendUdp: len=%d(0x%x) %d => ", tlen, tlen, __REV16(udp.SrcPort));
	RemoteIP.Show();
	debug_printf(":%d ", RemotePort);
	if(tlen > 0) Sys.ShowHex(udp.Next(), tlen > 64 ? 64 : tlen, false);
	debug_printf("\r\n");

	Tip->SendIP(IP_UDP, RemoteIP, (byte*)&udp, tlen);
}

// 发送UDP数据到目标地址
void UdpSocket::Send(ByteArray& bs, IPAddress& ip, ushort port)
{
	if(!ip.IsAny())
	{
		RemoteIP = ip;
		RemotePort = port;
	}

	Stream ms(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER) + bs.Length());
	ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER));
	
	UDP_HEADER* udp = ms.Retrieve<UDP_HEADER>();
	udp->Init(true);

	// 复制数据，确保数据不会溢出
	ms.Write(bs);

	// 发送的时候采用LocalPort
	LocalPort = BindPort;

	Send(*udp, bs.Length(), true);
}

bool UdpSocket::OnWrite(const byte* buf, uint len)
{
	ByteArray bs(buf, len);
	Send(bs);
	return len;
}

uint UdpSocket::OnRead(byte* buf, uint len)
{
	// 暂时不支持
	assert_param(false);
	return 0;
}
