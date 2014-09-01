#include "TinyIP.h"

#define NET_DEBUG DEBUG

const byte g_FullMac[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
const byte g_ZeroMac[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

TinyIP::TinyIP(ITransport* port, byte ip[4], byte mac[6])
{
	_port = port;

	byte defip[] = {192, 168, 0, 1};
	if(ip)
		memcpy(IP, ip, 4);
	else
	{
		// 随机IP，取ID最后一个字节
		memcpy(IP, defip, 3);
		IP[3] = Sys.ID[0];
	}

	if(mac)
		memcpy(Mac, mac, 6);
	else
	{
		// 随机Mac，前三个字节取自YWS的ASCII，最后3个字节取自后三个ID
		//Mac[0] = 59; Mac[1] = 57; Mac[2] = 53;
		memcpy(Mac, "YWS", 3);
		memcpy(&Mac[3], (byte*)Sys.ID, 3);
	}

	byte mask[] = {0xFF, 0xFF, 0xFF, 0};
	memcpy(Mask, mask, 4);
	memcpy(DHCPServer, defip, 4);
	memcpy(Gateway, defip, 4);
	memcpy(DNSServer, defip, 4);

	Buffer = NULL;
	BufferSize = 1500;
	EnableBroadcast = true;

	//memset(Sockets, 0x00, ArrayLength(Sockets) * sizeof(Socket*));

	//_net = NULL;
}

TinyIP::~TinyIP()
{
	if(_port) delete _port;
    _port = NULL;

	if(Buffer) delete Buffer;
	Buffer = NULL;

	/*if(_net) delete _net;
	_net = NULL;*/
}

// 循环调度的任务
uint TinyIP::Fetch(byte* buf, uint len)
{
	if(!buf) buf = Buffer;
	if(!len) len = BufferSize;

	// 获取缓冲区的包
	len = _port->Read(buf, len);
	// 如果缓冲器里面没有数据则转入下一次循环
	if(len < sizeof(ETH_HEADER)/* || !_net->Unpack(len)*/) return 0;

	ETH_HEADER* eth = (ETH_HEADER*)buf;
	// 只处理发给本机MAC的数据包。此时进行目标Mac地址过滤，可能是广播包
	if(memcmp(eth->DestMac, Mac, 6) != 0
	&& memcmp(eth->DestMac, g_FullMac, 6) != 0
	&& memcmp(eth->DestMac, g_ZeroMac, 6) != 0
	) return 0;

	return len;
}

void TinyIP::Process(byte* buf, uint len)
{
	if(!buf) return;

	ETH_HEADER* eth = (ETH_HEADER*)buf;
#if NET_DEBUG
	/*debug_printf("Ethernet 0x%04X ", eth->Type);
	ShowMac(eth->SrcMac);
	debug_printf(" => ");
	ShowMac(eth->DestMac);
	debug_printf("\r\n");*/
#endif

	// 只处理发给本机MAC的数据包。此时不能进行目标Mac地址过滤，因为可能是广播包
	//if(memcmp(eth->DestMac, Mac, 6) != 0) return;
	// 这里复制Mac地址
	memcpy(RemoteMac, eth->SrcMac, 6);

	// 计算负载数据的长度
	len -= eth->Size();

	// 处理ARP
	if(eth->Type == ETH_ARP)
	{
		//ProcessArp(buf, len);
		if(Arp) Arp->Process(buf, len);

		return;
	}

	IP_HEADER* ip = (IP_HEADER*)eth->Next();
	// 是否发给本机。注意memcmp相等返回0
	if(!ip || !IsMyIP(ip->DestIP)) return;

#if NET_DEBUG
	if(eth->Type != ETH_IP)
	{
		debug_printf("Unkown EthernetType 0x%02X From", eth->Type);
		ShowIP(ip->SrcIP);
		debug_printf("\r\n");
	}
#endif

	// 记录远程信息
	memcpy(RemoteIP, ip->SrcIP, 4);

	//!!! 太杯具了，收到的数据包可能有多余数据，这两个长度可能不等
	//assert_param(__REV16(ip->TotalLength) == len);
	// 数据包是否完整
	if(len < __REV16(ip->TotalLength)) return;
	// 计算负载数据的长度，注意ip可能变长，长度Length的单位是4字节
	//len -= sizeof(IP_HEADER);

	// 前面的len不准确，必须以这个为准
	uint size = __REV16(ip->TotalLength) - (ip->Length << 2);
	len = size;
	buf += (ip->Length << 2);

	for(int i=0; i < Sockets.Count(); i++)
	{
		Socket* socket = Sockets[i];
		if(socket)
		{
			// 必须类型匹配
			if(socket->Type == ip->Protocol)
			{
				// 如果处理成功，则中断遍历
				if(socket->Process(buf, len)) return;
			}
		}
	}

#if NET_DEBUG
	debug_printf("IP Unkown Protocol=%d ", ip->Protocol);
	ShowIP(ip->SrcIP);
	debug_printf(" => ");
	ShowIP(ip->DestIP);
	debug_printf("\r\n");
#endif
}

// 任务函数
void TinyIP::Work(void* param)
{
	TinyIP* tip = (TinyIP*)param;
	if(tip)
	{
		uint len = tip->Fetch();
		if(len)
		{
#if NET_DEBUG
			//ulong start = Time.Current();
#endif

			tip->Process(tip->Buffer, len);
#if NET_DEBUG
			//uint cost = Time.Current() - start;
			//debug_printf("TinyIP::Process cost %d us\r\n", cost);
#endif
		}
	}
}

bool TinyIP::Open()
{
	if(_port->Open()) return true;

	debug_printf("TinyIP Init Failed!\r\n");
	return false;
}

bool TinyIP::Init()
{
#if NET_DEBUG
	debug_printf("\r\nTinyIP Init...\r\n");
	uint us = Time.Current();
#endif

	// 分配缓冲区。比较大，小心堆空间不够
	if(!Buffer)
	{
		Buffer = new byte[BufferSize];
		assert_param(Buffer);
		assert_param(Sys.CheckMemory());

		// 首次使用时初始化缓冲区
		//if(!_net) _net = new NetPacker(Buffer);
	}

	if(!Open()) return false;

#if NET_DEBUG
	debug_printf("\tIP:\t");
	ShowIP(IP);
	debug_printf("\r\n\tMask:\t");
	ShowIP(Mask);
	debug_printf("\r\n\tGate:\t");
	ShowIP(Gateway);
	debug_printf("\r\n\tDHCP:\t");
	ShowIP(DHCPServer);
	debug_printf("\r\n\tDNS:\t");
	ShowIP(DNSServer);
	debug_printf("\r\n");
#endif

	// 添加到系统任务，马上开始，尽可能多被调度
    Sys.AddTask(Work, this);

#if NET_DEBUG
	us = Time.Current() - us;
	debug_printf("TinyIP Ready! Cost:%dms\r\n\r\n", us / 1000);
#endif

	return true;
}

void TinyIP::SendEthernet(ETH_TYPE type, byte* buf, uint len)
{
	ETH_HEADER* eth = (ETH_HEADER*)(buf - sizeof(ETH_HEADER));
	assert_param(IS_ETH_TYPE(type));

	eth->Type = type;
	memcpy(eth->DestMac, RemoteMac, 6);
	memcpy(eth->SrcMac, Mac, 6);

	len += sizeof(ETH_HEADER);
	//if(len < 60) len = 60;	// 以太网最小包60字节

	/*debug_printf("SendEthernet: type=0x%04x, len=%d\r\n", type, len);
	Sys.ShowHex((byte*)eth, len, '-');
	debug_printf("\r\n");*/

	//assert_param((byte*)eth == Buffer);
	_port->Write((byte*)eth, len);
}

void TinyIP::SendIP(IP_TYPE type, byte* buf, uint len)
{
	IP_HEADER* ip = (IP_HEADER*)(buf - sizeof(IP_HEADER));
	assert_param(ip);
	assert_param(IS_IP_TYPE(type));

	memcpy(ip->DestIP, RemoteIP, 4);
	memcpy(ip->SrcIP, IP, 4);

	ip->Version = 4;
	//ip->TypeOfService = 0;
	ip->Length = sizeof(IP_HEADER) / 4;	// 暂时不考虑附加数据
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + len);
	ip->Flags = 0x40;
	ip->FragmentOffset = 0;
	ip->TTL = 64;
	ip->Protocol = type;

	// 网络序是大端
	ip->Checksum = 0;
	ip->Checksum = __REV16((ushort)TinyIP::CheckSum((byte*)ip, sizeof(IP_HEADER), 0));

	SendEthernet(ETH_IP, (byte*)ip, sizeof(IP_HEADER) + len);
}

bool IcmpSocket::Process(byte* buf, uint len)
{
	if(len < sizeof(ICMP_HEADER)) return false;

	ICMP_HEADER* icmp = (ICMP_HEADER*)buf;
	len -= icmp->Size();
	//if(!icmp) return;
	//assert_param(_net->Payload == icmp->Next());

	if(OnPing)
	{
		// 返回值指示是否向对方发送数据包
		bool rs = OnPing(this, icmp, icmp->Next(), len);
		if(!rs) return true;
	}
	else
	{
#if NET_DEBUG
		if(icmp->Type != 0)
			debug_printf("Ping From "); // 打印发方的ip
		else
			debug_printf("Ping Reply "); // 打印发方的ip
		Tip->ShowIP(Tip->RemoteIP);
		//debug_printf(" len=%d Payload=%d ", len, _net->PayloadLength);
		debug_printf(" Payload=%d ", len);
		// 越过2个字节标识和2字节序列号
		debug_printf("ID=0x%04X Seq=0x%04X ", __REV16(icmp->Identifier), __REV16(icmp->Sequence));
		// 校验码验证通过
		/*ushort oldsum = icmp->Checksum;
		icmp->Checksum = 0;
		ushort chksum = (ushort)TinyIP::CheckSum((byte*)icmp, sizeof(ICMP_HEADER) + len, 0);
		icmp->Checksum = oldsum;
		debug_printf("Checksum=0x%04X Checksum=0x%04X ", icmp->Checksum, __REV16(chksum));*/
		//Sys.ShowString(_net->Payload, _net->PayloadLength);
		Sys.ShowString(icmp->Next(), len);
		debug_printf(" \r\n");
		//Sys.ShowHex(Buffer, len + sizeof(ICMP_HEADER) + sizeof(IP_HEADER) + sizeof(ETH_HEADER), '-');
		//debug_printf(" \r\n");
#endif
	}

	// 只处理ECHO请求
	if(icmp->Type != 8) return true;

	icmp->Type = 0; // 响应
	// 因为仅仅改变类型，因此我们能够提前修正校验码
	icmp->Checksum += 0x08;

	// 这里不能直接用sizeof(ICMP_HEADER)，而必须用len，因为ICMP包后面一般有附加数据
    Tip->SendIP(IP_ICMP, (byte*)icmp, icmp->Size() + len);

	return true;
}

// Ping目的地址，附带a~z重复的负载数据
bool IcmpSocket::Ping(byte ip[4], uint payloadLength)
{
	//byte* buf = Buffer;
	byte buf[sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(ICMP_HEADER) + 64];
	uint bufSize = ArrayLength(buf);

	ETH_HEADER* eth = (ETH_HEADER*)buf;
	IP_HEADER* _ip = (IP_HEADER*)eth->Next();
	ICMP_HEADER* icmp = (ICMP_HEADER*)_ip->Next();
	icmp->Init(true);

	uint count = 0;

	const byte* mac = Tip->Arp->Resolve(ip);
	if(!mac)
	{
#if NET_DEBUG
		debug_printf("No Mac For ");
		Tip->ShowIP(ip);
		debug_printf("\r\n");
#endif
		return false;
	}

	memcpy(Tip->RemoteMac, mac, 6);
	memcpy(Tip->RemoteIP, ip, 4);

	icmp->Type = 8;
	icmp->Code = 0;

	byte* data = icmp->Next();
	for(int i=0, k=0; i<payloadLength; i++, k++)
	{
		if(k >= 23) k-=23;
		*data++ = ('a' + k);
	}
	//_net->PayloadLength = payloadLength;

	ushort now = Time.Current() / 1000;
	ushort id = __REV16(Sys.ID[0]);
	ushort seq = __REV16(now);
	icmp->Identifier = id;
	icmp->Sequence = seq;

	icmp->Checksum = 0;
	icmp->Checksum = __REV16((ushort)TinyIP::CheckSum((byte*)icmp, sizeof(ICMP_HEADER) + payloadLength, 0));

#if NET_DEBUG
	debug_printf("Ping ");
	Tip->ShowIP(ip);
	debug_printf(" with Identifier=0x%04x Sequence=0x%04x\r\n", id, seq);
#endif
	Tip->SendIP(IP_ICMP, buf, sizeof(ICMP_HEADER) + payloadLength);

	// 总等待时间
	ulong end = Time.NewTicks(1 * 1000000);
	while(end > Time.CurrentTicks())
	{
		// 阻塞其它任务，频繁调度OnWork，等待目标数据
		uint len = Tip->Fetch(buf, bufSize);
		if(!len) continue;

		if(eth->Type == ETH_IP && _ip->Protocol == IP_ICMP)
		{
			if(icmp->Identifier == id && icmp->Sequence == seq
			&& memcmp(_ip->DestIP, Tip->IP, 4) == 0
			&& memcmp(_ip->SrcIP, ip, 4) == 0)
			{
				count++;
				break;
			}
		}

		// 用不到数据包交由系统处理
		Tip->Process(buf, len);
	}

	return count;
}

bool TcpSocket::Process(byte* buf, uint len)
{
	if(len < sizeof(TCP_HEADER)) return false;

	TCP_HEADER* tcp = (TCP_HEADER*)buf;
	//if(!tcp) return;
	Tcp = tcp;

	assert_param((tcp->Length << 2) >= sizeof(TCP_HEADER));
	len -= tcp->Length << 2;
	//assert_param(len == _net->PayloadLength);
	//assert_param(_net->Payload == tcp->Next());

	Port = __REV16(tcp->DestPort);
	RemotePort = __REV16(tcp->SrcPort);

#if NET_DEBUG
	/*debug_printf("TCP ");
	ShowIP(RemoteIP);
	debug_printf(":%d => ", __REV16(tcp->SrcPort));
	ShowIP(_net->IP->DestIP);
	debug_printf(":%d\r\n", __REV16(tcp->DestPort));*/
#endif

	// 第一次同步应答
	if (tcp->Flags & TCP_FLAGS_SYN) // SYN连接请求标志位，为1表示发起连接的请求数据包
	{
		if(OnAccepted)
			OnAccepted(this, tcp, tcp->Next(), len);
		else
		{
#if NET_DEBUG
			debug_printf("Tcp Accept "); // 打印发送方的ip
			TinyIP::ShowIP(Tip->RemoteIP);
			debug_printf("\r\n");
#endif
		}

		//第二次同步应答
		Head(1, true, false);

		// 需要用到MSS，所以采用4个字节的可选段
		Send(tcp, 4, TCP_FLAGS_SYN | TCP_FLAGS_ACK);

		return true;
	}
	// 第三次同步应答,三次应答后方可传输数据
	if (tcp->Flags & TCP_FLAGS_ACK) // ACK确认标志位，为1表示此数据包为应答数据包
	{
		// 无数据返回ACK
		if (len == 0)
		{
			if (tcp->Flags & (TCP_FLAGS_FIN | TCP_FLAGS_RST))      //FIN结束连接请求标志位。为1表示是结束连接的请求数据包
			{
				Head(1, false, true);
				Send(tcp, 0, TCP_FLAGS_ACK);
			}
			return true;
		}

		if(OnReceived)
		{
			// 返回值指示是否向对方发送数据包
			bool rs = OnReceived(this, tcp, tcp->Next(), len);
			if(!rs)
			{
				// 发送ACK，通知已收到
				Head(1, false, true);
				Send(tcp, 0, TCP_FLAGS_ACK);
				return true;
			}
		}
		else
		{
#if NET_DEBUG
			debug_printf("Tcp Data(%d) From ", len);
			TinyIP::ShowIP(RemoteIP);
			debug_printf(" : ");
			//Sys.ShowString(_net->Payload, len);
			Sys.ShowString(tcp->Next(), len);
			debug_printf("\r\n");
#endif
		}
		// 发送ACK，通知已收到
		Head(len, false, true);
		//Send(buf, 0, TCP_FLAGS_ACK);

		//TcpSend(buf, len);

		// 响应Ack和发送数据一步到位
		Send(tcp, len, TCP_FLAGS_ACK | TCP_FLAGS_PUSH);
	}
	else if(tcp->Flags & (TCP_FLAGS_FIN | TCP_FLAGS_RST))
	{
		if(OnDisconnected) OnDisconnected(this, tcp, tcp->Next(), len);

		// RST是对方紧急关闭，这里啥都不干
		if(tcp->Flags & TCP_FLAGS_FIN)
		{
			Head(1, false, true);
			//Close(tcp, 0);
			Send(tcp, 0, TCP_FLAGS_ACK | TCP_FLAGS_PUSH | TCP_FLAGS_FIN);
		}
	}

	return true;
}

void TcpSocket::Send(TCP_HEADER* tcp, uint len, byte flags)
{
	tcp->SrcPort = __REV16(Port);
	tcp->DestPort = __REV16(RemotePort);
    tcp->Flags = flags;
	if(tcp->Length < sizeof(TCP_HEADER) / 4) tcp->Length = sizeof(TCP_HEADER) / 4;

	// 网络序是大端
	tcp->Checksum = 0;
	tcp->Checksum = __REV16((ushort)TinyIP::CheckSum((byte*)tcp - 8, 8 + sizeof(TCP_HEADER) + len, 2));

	//assert_param(_net->IP);
	Tip->SendIP(IP_TCP, (byte*)tcp, tcp->Size() + len);
}

void TcpSocket::Head(uint ackNum, bool mss, bool opSeq)
{
    /*
	第一次握手：主机A发送位码为syn＝1，随机产生seq number=1234567的数据包到服务器，主机B由SYN=1知道，A要求建立联机；
	第二次握手：主机B收到请求后要确认联机信息，向A发送ack number=(主机A的seq+1)，syn=1，ack=1，随机产生seq=7654321的包；
	第三次握手：主机A收到后检查ack number是否正确，即第一次发送的seq number+1，以及位码ack是否为1，若正确，主机A会再发送ack number=(主机B的seq+1)，ack=1，主机B收到后确认seq值与ack=1则连接建立成功。
	完成三次握手，主机A与主机B开始传送数据。
	*/
	TCP_HEADER* tcp = Tcp;
	int ack = tcp->Ack;
	tcp->Ack = __REV(__REV(tcp->Seq) + ackNum);
    if (!opSeq)
    {
		// 我们仅仅递增第二个字节，这将允许我们以256或者512字节来发包
		tcp->Seq = __REV(seqnum << 8);
        // step the inititial seq num by something we will not use
        // during this tcp session:
        seqnum += 2;
    }else
	{
		tcp->Seq = ack;
	}

	tcp->Length = sizeof(TCP_HEADER) / 4;
    // 头部后面可能有可选数据，Length决定头部总长度（4的倍数）
    if (mss)
    {
        // 使用可选域设置 MSS 到 1460:0x5b4
		*(uint *)((byte*)tcp + sizeof(TCP_HEADER)) = __REV(0x020405b4);

		tcp->Length++;
    }
}

void TcpSocket::Ack(uint len)
{
	TCP_HEADER* tcp = (TCP_HEADER*)(Tip->Buffer + sizeof(ETH_HEADER) + sizeof(IP_HEADER));
	tcp->Init(true);
	Send(tcp, len, TCP_FLAGS_ACK | TCP_FLAGS_PUSH);
}

void TcpSocket::Close()
{
	TCP_HEADER* tcp = (TCP_HEADER*)(Tip->Buffer + sizeof(ETH_HEADER) + sizeof(IP_HEADER));
	tcp->Init(true);
	Send(tcp, 0, TCP_FLAGS_ACK | TCP_FLAGS_PUSH | TCP_FLAGS_FIN);
}

void TcpSocket::Send(byte* buf, uint len)
{
	TCP_HEADER* tcp = (TCP_HEADER*)(Tip->Buffer + sizeof(ETH_HEADER) + sizeof(IP_HEADER));
	tcp->Init(true);
	byte* end = Tip->Buffer + Tip->BufferSize;
	if(buf < tcp->Next() || buf >= end)
	{
		// 复制数据，确保数据不会溢出
		uint len2 = Tip->BufferSize - tcp->Offset() - tcp->Size();
		assert_param(len <= len2);

		memcpy(tcp->Next(), buf, len);
	}

	Send(tcp, len, TCP_FLAGS_PUSH);
}

bool UdpSocket::Process(byte* buf, uint len)
{
	if(len < sizeof(UDP_HEADER)) return false;
	len -= sizeof(UDP_HEADER);

	UDP_HEADER* udp = (UDP_HEADER*)buf;

	Tip->Port = __REV16(udp->DestPort);
	Tip->RemotePort = __REV16(udp->SrcPort);
	//byte* data = _net->Payload;
	//assert_param(data == udp->Next());
	byte* data = udp->Next();
	//assert_param(len == _net->PayloadLength);
	assert_param(len + sizeof(UDP_HEADER) == __REV16(udp->Length));

	if(OnReceived)
	{
		// 返回值指示是否向对方发送数据包
		bool rs = OnReceived(this, udp, udp->Next(), len);
		if(!rs) return true;
	}
	else
	{
#if NET_DEBUG
		IP_HEADER* ip = udp->Prev();
		debug_printf("UDP ");
		Tip->ShowIP(ip->SrcIP);
		debug_printf(":%d => ", Tip->RemotePort);
		Tip->ShowIP(ip->DestIP);
		debug_printf(":%d Payload=%d udp_len=%d \r\n", __REV16(udp->DestPort), len, __REV16(udp->Length));

		Sys.ShowString(data, len);
		debug_printf(" \r\n");
#endif
	}

	//udp->DestPort = udp->SrcPort;
	assert_param(data == (byte*)udp + sizeof(UDP_HEADER));
	//memcpy((byte*)udp + sizeof(UDP_HEADER), data, _net->PayloadLength);
	memcpy((byte*)udp + sizeof(UDP_HEADER), data, len);

	Send(buf, len, false);

	return true;
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

	//assert_param(_net->IP);
	Tip->SendIP(IP_UDP, (byte*)udp, sizeof(UDP_HEADER) + len);
}

void TinyIP::ShowIP(const byte* ip)
{
	debug_printf("%d", *ip++);
	for(int i=1; i<4; i++)
		debug_printf(".%d", *ip++);
}

void TinyIP::ShowMac(const byte* mac)
{
	debug_printf("%02X", *mac++);
	for(int i=1; i<6; i++)
		debug_printf("-%02X", *mac++);
}

uint TinyIP::CheckSum(byte* buf, uint len, byte type)
{
    // type 0=ip
    //      1=udp
    //      2=tcp
    unsigned long sum = 0;

    if(type == 1)
    {
        sum += IP_UDP; // protocol udp
        // the length here is the length of udp (data+header len)
        // =length given to this function - (IP.scr+IP.dst length)
        sum += len - 8; // = real tcp len
    }
    if(type == 2)
    {
        sum += IP_TCP;
        // the length here is the length of tcp (data+header len)
        // =length given to this function - (IP.scr+IP.dst length)
        sum += len - 8; // = real tcp len
    }
    // build the sum of 16bit words
    while(len > 1)
    {
        sum += 0xFFFF & (*buf << 8 | *(buf + 1));
        buf += 2;
        len -= 2;
    }
    // if there is a byte left then add it (padded with zero)
    if (len)
    {
        sum += (0xFF & *buf) << 8;
    }
    // now calculate the sum over the bytes in the sum
    // until the result is only 16bit long
    while (sum>>16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    // build 1's complement:
    return( (uint) sum ^ 0xFFFF);
}

bool TinyIP::IsMyIP(const byte ip[4])
{
	int i = 0;
	for(i = 0; i < 4 && ip[i] == IP[i]; i++);
	if(i == 4) return true;

	if(EnableBroadcast && IsBroadcast(ip)) return true;

	return false;
}

bool TinyIP::IsBroadcast(const byte ip[4])
{
	int i = 0;
	// 全网广播
	for(i = 0; i < 4 && ip[i] == 0xFF; i++);
	if(i == 4) return true;

	// 子网广播。网络位不变，主机位全1
	for(i = 0; i < 4 && ip[i] == (IP[i] | ~Mask[i]); i++);
	if(i == 4) return true;

	return false;
}

#define TinyIP_DHCP
#ifdef TinyIP_DHCP
void Dhcp::SendDhcp(DHCP_HEADER* dhcp, uint len)
{
	byte* p = dhcp->Next();
	if(p[len - 1] != DHCP_OPT_End)
	{
		// 此时指向的是负载数据后的第一个字节，所以第一个opt不许Next
		DHCP_OPT* opt = (DHCP_OPT*)(p + len);
		opt = opt->SetClientId(Tip->Mac, 6);
		opt = opt->Next()->SetData(DHCP_OPT_RequestedIP, Tip->IP, 4);
		opt = opt->Next()->SetData(DHCP_OPT_HostName, (byte*)"YWS_SmartOS", 11);
		opt = opt->Next()->SetData(DHCP_OPT_Vendor, (byte*)"http://www.NewLifeX.com", 23);
		byte ps[] = { 0x01, 0x06, 0x03, 0x2b}; // 需要参数 Mask/DNS/Router/Vendor
		opt = opt->Next()->SetData(DHCP_OPT_ParameterList, ps, ArrayLength(ps));
		opt = opt->Next()->End();

		len = (byte*)opt + 1 - p;
	}

	memcpy(dhcp->ClientMac, Tip->Mac, 6);

	memset(Tip->RemoteMac, 0xFF, 6);
	//memset(IP, 0x00, 6);
	memset(Tip->RemoteIP, 0xFF, 6);
	Tip->Port = 68;
	Tip->RemotePort = 67;

	// 如果最后一个字节不是DHCP_OPT_End，则增加End
	//byte* p = (byte*)dhcp + sizeof(DHCP_HEADER);
	//if(p[len - 1] != DHCP_OPT_End) p[len++] = DHCP_OPT_End;

	Send((byte*)dhcp, sizeof(DHCP_HEADER) + len, false);
}

// 获取选项，返回数据部分指针
DHCP_OPT* GetOption(byte* p, int len, DHCP_OPTION option)
{
	byte* end = p + len;
	while(p < end)
	{
		byte opt = *p++;
		byte len = *p++;
		if(opt == DHCP_OPT_End) return 0;
		if(opt == option) return (DHCP_OPT*)(p - 2);

		p += len;
	}

	return 0;
}

// 设置选项
void SetOption(byte* p, int len)
{
}

// 找服务器
void Dhcp::Discover(DHCP_HEADER* dhcp)
{
	byte* p = dhcp->Next();
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Discover);

	SendDhcp(dhcp, (byte*)opt->Next() - p);
}

void Dhcp::Request(DHCP_HEADER* dhcp)
{
	byte* p = dhcp->Next();
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Request);
	opt = opt->Next()->SetData(DHCP_OPT_DHCPServer, Tip->DHCPServer, 4);

	SendDhcp(dhcp, (byte*)opt->Next() - p);
}

void Dhcp::PareOption(byte* buf, uint len)
{
	byte* p = buf;
	byte* end = p + len;
	while(p < end)
	{
		byte opt = *p++;
		if(opt == DHCP_OPT_End) break;
		byte len = *p++;
		switch(opt)
		{
			case DHCP_OPT_Mask: memcpy(Tip->Mask, p, len); break;
			case DHCP_OPT_DNSServer: memcpy(Tip->DNSServer, p, len); break;
			case DHCP_OPT_Router: memcpy(Tip->Gateway, p, len); break;
			case DHCP_OPT_DHCPServer: memcpy(Tip->DHCPServer, p, len); break;
#if NET_DEBUG
			//default:
			//	debug_printf("Unkown DHCP Option=%d Length=%d\r\n", opt, len);
#endif
		}
		p += len;
	}
}

void RenewDHCP(void* param)
{
	TinyIP* tip = (TinyIP*)param;
	if(tip)
	{
		Dhcp dhcp(tip);
		if(!dhcp.Start())
		{
			debug_printf("TinyIP DHCP Fail!\r\n\r\n");
			return;
		}
	}
}

bool Dhcp::Start()
{
	uint dhcpid = (uint)Time.CurrentTicks();

	byte buf[400];
	uint bufSize = ArrayLength(buf);

	ETH_HEADER*  eth  = (ETH_HEADER*) buf;
	IP_HEADER*   ip   = (IP_HEADER*)  eth->Next();
	UDP_HEADER*  udp  = (UDP_HEADER*) ip->Next();
	DHCP_HEADER* dhcp = (DHCP_HEADER*)udp->Next();

	ulong next = 0;
	// 总等待时间
	ulong end = Time.NewTicks(10 * 1000000);
	while(end > Time.CurrentTicks())
	{
		// 得不到就重新发广播
		if(next < Time.CurrentTicks())
		{
			// 向DHCP服务器广播
			debug_printf("DHCP Discover...\r\n");
			dhcp->Init(dhcpid, true);
			Discover(dhcp);

			next = Time.NewTicks(1 * 1000000);
		}

		uint len = Tip->Fetch(buf, bufSize);
        // 如果缓冲器里面没有数据则转入下一次循环
        if(!len) continue;

		if(eth->Type != ETH_IP || ip->Protocol != IP_UDP) continue;

		if(__REV16(udp->DestPort) != 68) continue;

		// DHCP附加数据的长度
		//len -= dhcp->Offset() + dhcp->Size();
		len -= dhcp->Next() - buf;
		if(len <= 0) continue;

		if(!dhcp->Valid())continue;

		byte* data = dhcp->Next();

		// 获取DHCP消息类型
		DHCP_OPT* opt = GetOption(data, len, DHCP_OPT_MessageType);
		if(!opt) continue;

		if(opt->Data == DHCP_TYPE_Offer)
		{
			if(__REV(dhcp->TransID) == dhcpid)
			{
				memcpy(Tip->IP, dhcp->YourIP, 4);
				PareOption(dhcp->Next(), len);

				// 向网络宣告已经确认使用哪一个DHCP服务器提供的IP地址
				// 这里其实还应该发送ARP包确认IP是否被占用，如果被占用，还需要拒绝服务器提供的IP，比较复杂，可能性很低，暂时不考虑
#if NET_DEBUG
				debug_printf("DHCP Offer IP:");
				TinyIP::ShowIP(Tip->IP);
				debug_printf(" From ");
				TinyIP::ShowIP(ip->SrcIP);
				debug_printf("\r\n");
#endif

				dhcp->Init(dhcpid, true);
				Request(dhcp);
			}
		}
		else if(opt->Data == DHCP_TYPE_Ack)
		{
#if NET_DEBUG
			debug_printf("DHCP Ack   IP:");
			TinyIP::ShowIP(dhcp->YourIP);
			debug_printf(" From ");
			TinyIP::ShowIP(ip->SrcIP);
			debug_printf("\r\n");
#endif

			if(memcmp(dhcp->YourIP, Tip->IP, 4) == 0)
			{
				//IPIsReady = true;

				// 查找租约时间，提前续约
				opt = GetOption(data, len, DHCP_OPT_IPLeaseTime);
				if(opt)
				{
					// 续约时间，大字节序，时间单位秒
					uint time = __REV(*(uint*)&opt->Data);
					// DHCP租约过了一半以后重新获取IP地址
					if(time > 0) Sys.AddTask(RenewDHCP, this, time / 2 * 1000000, -1);
				}

				return true;
			}
		}
#if NET_DEBUG
		else if(opt->Data == DHCP_TYPE_Nak)
		{
			// 导致Nak的原因
			opt = GetOption(data, len, DHCP_OPT_Message);
			debug_printf("DHCP Nak   IP:");
			TinyIP::ShowIP(Tip->IP);
			debug_printf(" From ");
			TinyIP::ShowIP(ip->SrcIP);
			if(opt)
			{
				debug_printf(" ");
				Sys.ShowString(&opt->Data, opt->Length);
			}
			debug_printf("\r\n");
		}
		else
			debug_printf("DHCP Unkown Type=%d\r\n", opt->Data);
#endif
	}

	return false;
}
#endif

Socket::Socket(TinyIP* tip)
{
	assert_param(tip);

	Tip = tip;
	// 加入到列表
	//tip->Sockets.Add(this);
	__packed List<Socket*>* list = &tip->Sockets;
	list->Add(this);
}

Socket::~Socket()
{
	assert_param(Tip);

	// 从TinyIP中删除当前Socket
	Tip->Sockets.Remove(this);
}

ArpSocket::ArpSocket(TinyIP* tip) : Socket(tip)
{
	Type = ETH_ARP;

#ifdef STM32F0
	Count = 4;
#elif defined(STM32F1)
	Count = 16;
#elif defined(STM32F4)
	Count = 64;
#endif
	_Arps = NULL;
}

ArpSocket::~ArpSocket()
{
	if(_Arps) delete _Arps;
	_Arps = NULL;
}

bool ArpSocket::Process(byte* buf, uint len)
{
	ARP_HEADER* arp = (ARP_HEADER*)buf;
	if(!arp) return false;

	/*
	当封装的ARP报文在以太网上传输时，硬件类型字段赋值为0x0100，标识硬件为以太网硬件；
	协议类型字段赋值为0x0800，标识上次协议为IP协议；由于以太网的MAC地址为48比特位，IP地址为32比特位，则硬件地址长度字段赋值为6，协议地址长度字段赋值为4 ；
	选项字段标识ARP报文的类型，当为请求报文时，赋值为0x0100，当为回答报文时，赋值为0x0200。
	*/

	// 如果是Arp响应包，自动加入缓存
	/*if(arp->Option == 0x0200)
	{
		AddArp(arp->SrcIP, arp->SrcMac);
	}
	// 别人的响应包这里收不到呀，还是把请求包也算上吧
	if(arp->Option == 0x0100)
	{
		AddArp(arp->SrcIP, arp->SrcMac);
	}*/

	// 是否发给本机。注意memcmp相等返回0
	if(memcmp(arp->DestIP, Tip->IP, 4) !=0 ) return true;

	len -= sizeof(ARP_HEADER);
#if NET_DEBUG
	// 数据校验
	assert_param(arp->HardType == 0x0100);
	assert_param(arp->ProtocolType == ETH_IP);
	assert_param(arp->HardLength == 6);
	assert_param(arp->ProtocolLength == 4);
	assert_param(arp->Option == 0x0100);
	//assert_param(_net->PayloadLength == len);
	//assert_param(_net->Payload == arp->Next());

	if(arp->Option == 0x0100)
		debug_printf("ARP Request For ");
	else
		debug_printf("ARP Response For ");

	Tip->ShowIP(arp->DestIP);
	debug_printf(" <= ");
	Tip->ShowIP(arp->SrcIP);
	debug_printf(" [");
	Tip->ShowMac(arp->SrcMac);
	//debug_printf("] len=%d Payload=%d\r\n", len, _net->PayloadLength);
	debug_printf("] Payload=%d\r\n", len);
#endif
	// 是否发给本机
	//if(memcmp(arp->DestIP, IP, 4)) return;

	// 构造响应包
	arp->Option = 0x0200;
	// 来源IP和Mac作为目的地址
	memcpy(arp->DestMac, arp->SrcMac, 6);
	memcpy(arp->DestIP, arp->SrcIP, 4);
	memcpy(arp->SrcMac, Tip->Mac, 6);
	memcpy(arp->SrcIP, Tip->IP, 4);

#if NET_DEBUG
	debug_printf("ARP Response To ");
	Tip->ShowIP(arp->DestIP);
	debug_printf(" size=%d\r\n", sizeof(ARP_HEADER));
#endif

	Tip->SendEthernet(ETH_ARP, buf, sizeof(ARP_HEADER));

	return true;
}

// 请求Arp并返回其Mac。timeout超时3秒，如果没有超时时间，表示异步请求，不用等待结果
const byte* ArpSocket::Request(const byte ip[4], int timeout)
{
	//ARP_HEADER* arp = _net->SetARP();
	byte buf[sizeof(ETH_HEADER) + sizeof(ARP_HEADER)];
	uint bufSize = ArrayLength(buf);

	ETH_HEADER* eth = (ETH_HEADER*)buf;
	ARP_HEADER* arp = (ARP_HEADER*)eth->Next();

	// 构造请求包
	arp->Option = 0x0100;
	memcpy(arp->DestMac, g_ZeroMac, 6);
	memcpy(arp->DestIP, ip, 4);
	memcpy(arp->SrcMac, Tip->Mac, 6);
	memcpy(arp->SrcIP, Tip->IP, 4);

#if NET_DEBUG
	debug_printf("ARP Request To ");
	Tip->ShowIP(arp->DestIP);
	debug_printf(" size=%d\r\n", sizeof(ARP_HEADER));
#endif

	//byte* buf = Buffer;
	Tip->SendEthernet(ETH_ARP, buf, sizeof(ARP_HEADER));

	// 如果没有超时时间，表示异步请求，不用等待结果
	if(timeout <= 0) return NULL;

	// 总等待时间
	ulong end = Time.NewTicks(timeout * 1000000);
	while(end > Time.CurrentTicks())
	{
		// 阻塞其它任务，频繁调度OnWork，等待目标数据
		uint len = Tip->Fetch(buf, bufSize);
		if(!len) continue;

		// 处理ARP
		if(eth->Type == ETH_ARP)
		{
			// 是否目标发给本机的Arp响应包。注意memcmp相等返回0
			if(memcmp(arp->DestIP, Tip->IP, 4) == 0
			&& memcmp(arp->SrcIP, ip, 4) == 0
			&& arp->Option == 0x0200)
			{
				return arp->SrcMac;
			}
		}

		// 用不到数据包交由系统处理
		Tip->Process(buf, len);
	}

	return NULL;
}

const byte* ArpSocket::Resolve(const byte ip[4])
{
	if(Tip->IsBroadcast(ip)) return g_FullMac;

	// 如果不在本子网，那么应该找网关的Mac
	if(ip[0] & Tip->Mask[0] != Tip->IP[0] & Tip->Mask[0]
	|| ip[1] & Tip->Mask[1] != Tip->IP[1] & Tip->Mask[1]
	|| ip[2] & Tip->Mask[2] != Tip->IP[2] & Tip->Mask[2]
	|| ip[3] & Tip->Mask[3] != Tip->IP[3] & Tip->Mask[3]
	) ip = Tip->Gateway;
	// 下面的也可以，但是比较难理解
	/*if(ip[0] ^ IP[0] != ~Mask[0]
	|| ip[1] ^ IP[1] != ~Mask[1]
	|| ip[2] ^ IP[2] != ~Mask[2]
	|| ip[3] ^ IP[3] != ~Mask[3]
	) ip = Gateway;*/

	ARP_ITEM* item = NULL;	// 匹配项
	if(_Arps)
	{
		uint sNow = Time.Current() / 1000000;	// 当前时间，秒
		// 在表中查找
		for(int i=0; i<Count; i++)
		{
			ARP_ITEM* arp = &_Arps[i];
			if(memcmp(arp->IP, ip, 4) == 0)
			{
				// 如果未过期，则直接使用。否则重新请求
				if(arp->Time > sNow) return arp->Mac;

				// 暂时保存，待会可能请求失败，还可以用旧的顶着
				item = arp;
			}
		}
	}

	// 找不到则发送Arp请求。如果有旧值，则使用异步请求即可
	const byte* mac = Request(ip, item ? 0 : 3);
	if(!mac) return item ? item->Mac : NULL;

	Add(ip, mac);

	return mac;
}

void ArpSocket::Add(const byte ip[4], const byte mac[6])
{
#if NET_DEBUG
	debug_printf("Add Arp(");
	TinyIP::ShowIP(ip);
	debug_printf(", ");
	TinyIP::ShowMac(mac);
	debug_printf(")\r\n");
#endif

	if(!_Arps)
	{
		_Arps = new ARP_ITEM[Count];
		memset(_Arps, 0, sizeof(ARP_ITEM) * Count);
	}

	ARP_ITEM* item = NULL;
	ARP_ITEM* empty = NULL;
	// 在表中查找项
	byte ipnull[] = { 0, 0, 0, 0 };
	for(int i=0; i<Count; i++)
	{
		ARP_ITEM* arp = &_Arps[i];
		if(memcmp(arp->IP, ip, 4) == 0)
		{
			item = arp;
			break;
		}
		if(!empty && memcmp(arp->IP, ipnull, 4) == 0)
		{
			empty = arp;
			break;
		}
	}

	// 如果没有匹配项，则使用空项
	if(!item) item = empty;
	// 如果也没有空项，表示满了，那就替换最老的一个
	if(!item)
	{
		uint oldTime = 0xFFFFFF;
		// 在表中查找最老项用于替换
		for(int i=0; i<Count; i++)
		{
			ARP_ITEM* arp = &_Arps[i];
			// 找最老的一个，待会如果需要覆盖，就先覆盖它。避开网关
			if(arp->Time < oldTime && memcmp(arp->IP, Tip->Gateway, 4) != 0)
			{
				oldTime = arp->Time;
				item = arp;
			}
		}
#if NET_DEBUG
		debug_printf("Arp Table is full, replace ");
		TinyIP::ShowIP(item->IP);
		debug_printf("\r\n");
#endif
	}

	uint sNow = Time.Current() / 1000000;	// 当前时间，秒
	// 保存
	memcpy(item->IP, ip, 4);
	memcpy(item->Mac, mac, 6);
	item->Time = sNow + 60;	// 默认过期时间1分钟
}

TcpSocket::TcpSocket(TinyIP* tip) : Socket(tip)
{
	Type = IP_TCP;

	seqnum = 0xa;
}
