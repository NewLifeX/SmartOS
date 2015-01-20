#include "Udp.h"

#define NET_DEBUG DEBUG

UdpSocket::UdpSocket(TinyIP* tip) : Socket(tip)
{
	Type = IP_UDP;
	Port		= 0;
	RemoteIP	= 0;
	RemotePort	= 0;
	LocalIP		= 0;
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

bool UdpSocket::Process(MemoryStream* ms)
{
	UDP_HEADER* udp = ms->Retrieve<UDP_HEADER>();
	if(!udp) return false;

	ushort port = __REV16(udp->DestPort);
	ushort remotePort = __REV16(udp->SrcPort);

	// 仅处理本连接的IP和端口
	if(Port != 0 && port != Port) return false;

	IP_HEADER* ip = udp->Prev();
	RemotePort	= remotePort;
	RemoteIP	= ip->SrcIP;
	LocalPort	= port;
	LocalIP		= ip->DestIP;

#if NET_DEBUG
	byte* data = udp->Next();
	uint len = ms->Remain();
	uint plen = __REV16(udp->Length);
	assert_param(len + sizeof(UDP_HEADER) == plen);
	//Sys.ShowHex((byte*)udp, udp->Size(), '-');
	//debug_printf("\r\n");
#endif

	OnProcess(udp, *ms);

	return true;
}

void UdpSocket::OnProcess(UDP_HEADER* udp, MemoryStream& ms)
{
	byte* data = ms.Current();
	//uint len = ms.Remain();
	// 计算标称的数据长度
	uint len = __REV16(udp->Length) - sizeof(UDP_HEADER);
	assert_param(len <= ms.Remain());

#if NET_DEBUG
	/*ushort oldsum = __REV16(udp->Checksum);
	//udp->Checksum = 0;
	//udp->Checksum = __REV16(Tip->CheckSum((byte*)udp, sizeof(UDP_HEADER) + len, 1));
	ushort newsum = Tip->CheckSum((byte*)udp, sizeof(UDP_HEADER) + len, 1);
	debug_printf("UDP::Checksum ori=0x%02x new=0x%02x\r\n", oldsum, newsum);*/
#endif

	// 触发ITransport接口事件
	uint len2 = OnReceive(data, len);
	// 如果有返回，说明有数据要回复出去
	if(len2) Write(data, len2);

	if(OnReceived)
	{
		// 返回值指示是否向对方发送数据包
		bool rs = OnReceived(this, udp, data, len);
		if(rs) Send(udp, len, false);
	}
	else
	{
#if NET_DEBUG
		debug_printf("UDP ");
		Tip->ShowIP(RemoteIP);
		debug_printf(":%d => ", RemotePort);
		Tip->ShowIP(LocalIP);
		debug_printf(":%d Payload=%d udp_len=%d \r\n", LocalPort, len, __REV16(udp->Length));

		Sys.ShowString(data, len);
		debug_printf(" \r\n");

		//Send(udp, len, false);
#endif
	}
}

void UdpSocket::Send(UDP_HEADER* udp, uint len, bool checksum)
{
	//UDP_HEADER* udp = (UDP_HEADER*)(buf - sizeof(UDP_HEADER));
	assert_param(udp);

	uint tlen = sizeof(UDP_HEADER) + len;
	udp->SrcPort = __REV16(Port > 0 ? Port : LocalPort);
	udp->DestPort = __REV16(RemotePort);
	udp->Length = __REV16(tlen);

	// 必须在校验码之前设置，因为计算校验码需要地址
	Tip->RemoteIP = RemoteIP;

	// 网络序是大端
	udp->Checksum = 0;
	if(checksum) udp->Checksum = __REV16(Tip->CheckSum((byte*)udp, tlen, 1));

	// 不能注释UDP这行日志，否则DHCP失效
	debug_printf("SendUdp: len=%d(0x%x) %d => %d ", tlen, tlen, __REV16(udp->SrcPort), RemotePort);
	if(tlen > 0) Sys.ShowString(udp->Next(), tlen > 64 ? 64 : tlen, false);
	debug_printf("\r\n");

	Tip->SendIP(IP_UDP, (byte*)udp, tlen);
}

UDP_HEADER* UdpSocket::Create()
{
	return (UDP_HEADER*)(Tip->Buffer + sizeof(ETH_HEADER) + sizeof(IP_HEADER));
}

// 发送UDP数据到目标地址
void UdpSocket::Send(const byte* buf, uint len, IPAddress ip, ushort port)
{
	if(ip > 0)
	{
		RemoteIP = ip;
		RemotePort = port;
	}

	UDP_HEADER* udp = Create();
	udp->Init(true);
	byte* end = Tip->Buffer + Tip->BufferSize;
	if(buf < udp->Next() || buf >= end)
	{
		// 复制数据，确保数据不会溢出
		uint len2 = Tip->BufferSize - udp->Offset() - udp->Size();
		assert_param(len <= len2);

		memcpy(udp->Next(), buf, len);
	}

	// 发送的时候采用LocalPort
	LocalPort = BindPort;

	Send(udp, len, true);
}

bool UdpSocket::OnWrite(const byte* buf, uint len)
{
	Send(buf, len);
	return len;
}

uint UdpSocket::OnRead(byte* buf, uint len)
{
	// 暂时不支持
	assert_param(false);
	return 0;
}
