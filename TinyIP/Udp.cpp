#include "Udp.h"

UdpSocket::UdpSocket(TinyIP* tip) : Socket(tip)
{
	Type = IP_UDP;
	Port = 0;
	RemoteIP = 0;
	RemotePort = 0;
}

bool UdpSocket::Process(MemoryStream* ms)
{
	UDP_HEADER* udp = ms->Retrieve<UDP_HEADER>();
	if(!udp) return false;

	ushort port = __REV16(udp->DestPort);
	ushort remotePort = __REV16(udp->SrcPort);

	// 仅处理本连接的IP和端口
	if(Port != 0 && port != Port) return false;

	RemotePort = remotePort;
	RemoteIP = udp->Prev()->SrcIP;

#if NET_DEBUG
	byte* data = udp->Next();
	uint len = ms->Remain();
	uint plen = __REV16(udp->Length);
	assert_param(len + sizeof(UDP_HEADER) == plen);
#endif

	OnProcess(udp, *ms);

	return true;
}

void UdpSocket::OnProcess(UDP_HEADER* udp, MemoryStream& ms)
{
	byte* data = ms.Current();
	uint len = ms.Remain();

	// 触发ITransport接口事件
	len = OnReceive(data, len);
	// 如果有返回，说明有数据要回复出去
	if(len) Write(data, len);

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
		Tip->ShowIP(Tip->RemoteIP);
		debug_printf(":%d => ", Tip->RemotePort);
		Tip->ShowIP(Tip->LocalIP);
		debug_printf(":%d Payload=%d udp_len=%d \r\n", Tip->Port, len, __REV16(udp->Length));

		Sys.ShowString(data, len);
		debug_printf(" \r\n");

		Send(udp, len, false);
#endif
	}
}

void UdpSocket::Send(UDP_HEADER* udp, uint len, bool checksum)
{
	//UDP_HEADER* udp = (UDP_HEADER*)(buf - sizeof(UDP_HEADER));
	assert_param(udp);

	udp->SrcPort = __REV16(Port > 0 ? Port : Tip->Port);
	udp->DestPort = __REV16(RemotePort > 0 ? RemotePort : Tip->RemotePort);
	udp->Length = __REV16(sizeof(UDP_HEADER) + len);

	// 网络序是大端
	udp->Checksum = 0;
	if(checksum) udp->Checksum = __REV16((ushort)TinyIP::CheckSum((byte*)udp, sizeof(UDP_HEADER) + len, 1));

	Tip->RemoteIP = RemoteIP;

	debug_printf("SendUdp: len=%d(0x%x) %d => %d \r\n", udp->Length, udp->Length, __REV16(udp->SrcPort), __REV16(udp->DestPort));

	Tip->SendIP(IP_UDP, (byte*)udp, sizeof(UDP_HEADER) + len);
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
