#include "Udp.h"

bool UdpSocket::Process(MemoryStream* ms)
{
	UDP_HEADER* udp = ms->Retrieve<UDP_HEADER>();
	if(!udp) return false;

	ushort port = __REV16(udp->DestPort);
	ushort remotePort = __REV16(udp->SrcPort);

	// 仅处理本连接的IP和端口
	if(Port != 0 && port != Port) return false;

	Tip->Port = port;
	Tip->RemotePort = remotePort;

#if NET_DEBUG
	byte* data = udp->Next();
	uint len = ms->Remain();
	uint plen = __REV16(udp->Length);
	assert_param(len + sizeof(UDP_HEADER) == plen);
#endif

	OnReceive(udp, *ms);

	return true;
}

void UdpSocket::OnReceive(UDP_HEADER* udp, MemoryStream& ms)
{
	byte* data = ms.Current();
	uint len = ms.Remain();

	if(OnReceived)
	{
		// 返回值指示是否向对方发送数据包
		bool rs = OnReceived(this, udp, data, len);
		if(rs) Send(data, len, false);
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

		Send(data, len, false);
#endif
	}
}

void UdpSocket::Send(byte* buf, uint len, bool checksum)
{
	UDP_HEADER* udp = (UDP_HEADER*)(buf - sizeof(UDP_HEADER));
	assert_param(udp);

	udp->SrcPort = __REV16(Tip->Port);
	udp->DestPort = __REV16(Tip->RemotePort);
	udp->Length = __REV16(sizeof(UDP_HEADER) + len);

	// 网络序是大端
	udp->Checksum = 0;
	if(checksum) udp->Checksum = __REV16((ushort)TinyIP::CheckSum((byte*)udp, sizeof(UDP_HEADER) + len, 1));

	Tip->SendIP(IP_UDP, (byte*)udp, sizeof(UDP_HEADER) + len);
}
