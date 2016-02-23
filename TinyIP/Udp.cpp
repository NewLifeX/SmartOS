#include "Udp.h"

//#define NET_DEBUG DEBUG
#define NET_DEBUG 0

UdpSocket::UdpSocket(TinyIP* tip) : TinySocket(tip, IP_UDP)
{
	MaxSize	= 1500;
	Host	= tip;
	Protocol	= ProtocolType::Udp;

	// 累加端口
	static ushort g_udp_port = 1024;
	if(g_udp_port < 1024) g_udp_port = 1024;
	Local.Port	= g_udp_port++;
	Local.Address = tip->IP;
}

const char* UdpSocket::ToString() const
{
	static char name[10];
	sprintf(name, "UDP_%d", Local.Port);
	return name;
}

bool UdpSocket::OnOpen()
{
	debug_printf("Udp::Open %d\r\n", Local.Port);

	Enable = true;
	return Enable;
}

void UdpSocket::OnClose()
{
	debug_printf("Udp::Close %d\r\n", Local.Port);
	Enable = false;
}

bool UdpSocket::Process(IP_HEADER& ip, Stream& ms)
{
	UDP_HEADER* udp = ms.Retrieve<UDP_HEADER>();
	if(!udp) return false;

	ushort port = _REV16(udp->DestPort);
	ushort remotePort = _REV16(udp->SrcPort);

	// 仅处理本连接的IP和端口
	if(port != Local.Port) return false;

	CurRemote.Port		= remotePort;
	CurRemote.Address	= ip.SrcIP;
	CurLocal.Port		= port;
	CurLocal.Address	= ip.DestIP;

#if NET_DEBUG
	byte* data	= udp->Next();
	uint len	= ms.Remain();
	uint plen	= _REV16(udp->Length);
	assert_param2(len + sizeof(UDP_HEADER) == plen, "UDP数据包标称长度与实际不符");
#endif

	OnProcess(ip, *udp, ms);

	return true;
}

void UdpSocket::OnProcess(IP_HEADER& ip, UDP_HEADER& udp, Stream& ms)
{
	byte* data = ms.Current();
	//uint len = ms.Remain();
	// 计算标称的数据长度
	uint len = _REV16(udp.Length) - sizeof(UDP_HEADER);
	assert_param2(len <= ms.Remain(), "UDP数据包不完整");

	// 触发ITransport接口事件
	Array bs(data, len);
	uint len2 = OnReceive(bs, &CurRemote);
	// 如果有返回，说明有数据要回复出去
	//if(len2) Write(data, len2);
	if(len2)
	{
		Remote = CurRemote;
		Send(bs);
	}

	if(OnReceived)
	{

		// 返回值指示是否向对方发送数据包
		bool rs = OnReceived(*this, udp, CurRemote, ms);
		if(rs && ms.Remain() > 0) SendPacket(udp, len, CurRemote.Address, CurRemote.Port, false);
	}
	else
	{
#if NET_DEBUG
		debug_printf("UDP ");
		CurRemote.Show();
		debug_printf(" => ");
		CurLocal.Show();
		debug_printf(" Payload=%d udp_len=%d \r\n", len, _REV16(udp.Length));

		ByteArray(data, len).Show(true);
#endif
	}
}

void UdpSocket::SendPacket(UDP_HEADER& udp, uint len, IPAddress& ip, ushort port, bool checksum)
{
	uint tlen		= sizeof(UDP_HEADER) + len;
	udp.SrcPort		= _REV16(Local.Port);
	udp.DestPort	= _REV16(port);
	udp.Length		= _REV16(tlen);

	// 网络序是大端
	udp.Checksum	= 0;
	if(checksum) udp.Checksum = _REV16(Tip->CheckSum(&ip, (byte*)&udp, tlen, 1));

#if NET_DEBUG
	debug_printf("SendUdp: len=%d(0x%x) %d => ", tlen, tlen, _REV16(udp.SrcPort));
	ip.Show();
	debug_printf(":%d ", port);
	if(tlen > 0) ByteArray(udp.Next(), tlen > 64 ? 64 : tlen).Show();
	debug_printf("\r\n");
#else
	Sys.Sleep(1);
#endif

	Tip->SendIP(IP_UDP, ip, (byte*)&udp, tlen);
}

// 发送UDP数据到目标地址
bool UdpSocket::Send(const Array& bs)
{
	//if(ip.IsAny()) ip = Remote.Address;
	//if(!port) port = Remote.Port;
	Local.Address	= Tip->IP;

	//byte buf[sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER) + 1024];
	byte buf[1500];
	Stream ms(buf, ArrayLength(buf));
	ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER));

	UDP_HEADER* udp = ms.Retrieve<UDP_HEADER>();
	udp->Init(true);

	// 复制数据，确保数据不会溢出
	ms.Write(bs);

	SendPacket(*udp, bs.Length(), Remote.Address, Remote.Port, true);

	return true;
}

bool UdpSocket::OnWrite(const Array& bs)
{
	return Send(bs);
}

uint UdpSocket::Receive(Array& bs)
{
	return 0;
}

uint UdpSocket::OnRead(Array& bs)
{
	// 暂时不支持
	return 0;
}
