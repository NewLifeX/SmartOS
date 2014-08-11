#include "TinyIP.h"
#include "../Net/Net.h"

#define NET_DEBUG DEBUG

void ShowHex(byte* buf, int size)
{
	//while(size--) debug_printf("%02X-", *buf++);
	for(int i=0; i<size; i++)
	{
		debug_printf("%02X-", *buf++);
		if(((i + 1) & 0xF) == 0) debug_printf("\r\n");
	}
	debug_printf("\r\n");
}

TinyIP::TinyIP(Enc28j60* enc, byte ip[4], byte mac[6])
{
	_enc = enc;
	memcpy(IP, ip, 4);
	memcpy(Mac, mac, 6);
	byte mask[] = {0xFF, 0xFF, 0xFF, 0};
	memcpy(Mask, mask, 4);

	Buffer = NULL;
	BufferSize = 1500;

	seqnum = 0xa;

	// 分配缓冲区。比较大，小心栈空间不够
	if(!Buffer) Buffer = new byte[BufferSize + 1];
	assert_param(Buffer);
	assert_param(Sys.CheckMemory());

	_net = new NetPacker(Buffer);
}

TinyIP::~TinyIP()
{
    _enc = NULL;
	if(Buffer) delete Buffer;
	Buffer = NULL;

	if(_net) delete _net;
	_net = NULL;
}

void TinyIP::TcpClose(byte* buf, uint size)
{
	SendTcp(buf, size, TCP_FLAGS_ACK_V | TCP_FLAGS_PUSH_V | TCP_FLAGS_FIN_V);
}

void TinyIP::TcpSend(byte* buf, uint size)
{
	SendTcp(buf, size, TCP_FLAGS_ACK_V | TCP_FLAGS_PUSH_V);
}

// 循环调度的任务
void TinyIP::OnWork()
{
	byte* buf = Buffer;
	// 获取缓冲区的包
	uint len = _enc->PacketReceive(buf, BufferSize);
	// 如果缓冲器里面没有数据则转入下一次循环
	if(!_net->Unpack(len)) return;

	ETH_HEADER* eth = _net->Eth;
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

	// 处理ARP
	if(eth->Type == ETH_ARP)
	{
		ProcessArp(buf, len);
		return;
	}

	IP_HEADER* ip = _net->IP;
	// 是否发给本机。注意memcmp相等返回0
	if(!ip || memcmp(ip->DestIP, IP, 4) != 0) return;

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

	if(ip->Protocol == IP_ICMP)
	{
		ProcessICMP(buf, len);
		return;
	}
	if (ip->Protocol == IP_TCP)
	{
		ProcessTcp(buf, len);
		return;
	}
	if (ip->Protocol == IP_UDP /*&& buf[UDP_DST_PORT_H_P] == 4*/)
	{
		ProcessUdp(buf, len);
		return;
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
	if(tip) tip->OnWork();
}

bool TinyIP::Init()
{
#if NET_DEBUG
	debug_printf("\r\nTinyIP Init...\r\nT");
	uint us = Time.Current();
#endif

    // 初始化 enc28j60 的MAC地址(物理地址),这个函数必须要调用一次
    _enc->Init((string)Mac);

    // 将enc28j60第三引脚的时钟输出改为：from 6.25MHz to 12.5MHz(本例程该引脚NC,没用到)
    _enc->ClockOut(2);
	//Sys.Sleep(1000);

	if(UseDHCP)
	{
		IPIsReady = false;
		dhcp_id = (uint)Time.CurrentTicks();

		DHCPConfig(Buffer);
		if(!IPIsReady)
		{
#if NET_DEBUG
			debug_printf("TinyIP DHCP Fail!\r\n\r\n");
#endif
			return false;
		}
	}

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

void TinyIP::ProcessArp(byte* buf, uint len)
{
	ARP_HEADER* arp = _net->ARP;
	if(!arp) return;

	/*
	当封装的ARP报文在以太网上传输时，硬件类型字段赋值为0x0100，标识硬件为以太网硬件；
	协议类型字段赋值为0x0800，标识上次协议为IP协议；由于以太网的MAC地址为48比特位，IP地址为32比特位，则硬件地址长度字段赋值为6，协议地址长度字段赋值为4 ；
	选项字段标识ARP报文的类型，当为请求报文时，赋值为0x0100，当为回答报文时，赋值为0x0200。
	*/

	// 是否发给本机。注意memcmp相等返回0
	if(memcmp(arp->DestIP, IP, 4) !=0 ) return;

#if NET_DEBUG
	// 数据校验
	assert_param(arp->HardType == 0x0100);
	assert_param(arp->ProtocolType == ETH_IP);
	assert_param(arp->HardLength == 6);
	assert_param(arp->ProtocolLength == 4);
	assert_param(arp->Option == 0x0100);

	if(arp->Option == 0x0100)
		debug_printf("ARP Request For ");
	else
		debug_printf("ARP Response For ");

	ShowIP(arp->DestIP);
	debug_printf(" <= ");
	ShowIP(arp->SrcIP);
	debug_printf(" [");
	ShowMac(arp->SrcMac);
	debug_printf("] len=%d Payload=%d\r\n", len, _net->PayloadLength);
#endif
	// 是否发给本机
	//if(memcmp(arp->DestIP, IP, 4)) return;

	// 构造响应包
	arp->Option = 0x0200;
	// 来源IP和Mac作为目的地址
	memcpy(&arp->DestMac, &arp->SrcMac, 6);
	memcpy(&arp->DestIP, &arp->SrcIP, 4);
	memcpy(&arp->SrcMac, Mac, 6);
	memcpy(&arp->SrcIP, IP, 4);

#if NET_DEBUG
	debug_printf("ARP Response To ");
	ShowIP(arp->DestIP);
	debug_printf(" size=%d\r\n", sizeof(ARP_HEADER));
#endif

	_net->Eth->Type = ETH_ARP;
	SendEthernet(buf, sizeof(ARP_HEADER));
}

void TinyIP::ProcessICMP(byte* buf, uint len)
{
	ICMP_HEADER* icmp = _net->ICMP;
	if(!icmp) return;

	len -= ((byte*)icmp - (byte*)_net->Eth);

#if NET_DEBUG
	debug_printf("Ping From "); // 打印发方的ip
	ShowIP(RemoteIP);
	debug_printf(" len=%d Payload=%d ", len, _net->PayloadLength);
	// 越过2个字节标识和2字节序列号
	for(int i=4; i<_net->PayloadLength; i++)
		debug_printf("%c", _net->Payload[i]);
	debug_printf(" \r\n");
#endif

	// 只处理ECHO请求
	if(icmp->Type != 8) return;

	icmp->Type = 0; // 响应
	// 因为仅仅改变类型，因此我们能够提前修正校验码
	icmp->Checksum += 0x08;

	_net->IP->Protocol = IP_ICMP;
	// 这里不能直接用sizeof(ICMP_HEADER)，而必须用len，因为ICMP包后面一般有附加数据
    SendIP(buf, len);
}

void TinyIP::ProcessTcp(byte* buf, uint len)
{
	len -= sizeof(ETH_HEADER) + sizeof(IP_HEADER);
	if(len < sizeof(TCP_HEADER)) return;

	TCP_HEADER* tcp = _net->TCP;
	if(!tcp) return;

	len -= ((byte*)tcp - (byte*)_net->Eth);

	Port = __REV16(tcp->DestPort);
	RemotePort = __REV16(tcp->SrcPort);

#if NET_DEBUG
	debug_printf("TCP ");
	ShowIP(RemoteIP);
	debug_printf(":%d => ", __REV16(tcp->SrcPort));
	ShowIP(_net->IP->DestIP);
	debug_printf(":%d\r\n", __REV16(tcp->DestPort));
#endif

	// 第一次同步应答
	if (tcp->Flags & TCP_FLAGS_SYN_V) // SYN连接请求标志位，为1表示发起连接的请求数据包
	{
		debug_printf("\tRequest From "); // 打印发送方的ip
		ShowIP(RemoteIP);
		debug_printf("\r\n");

		//第二次同步应答
		make_tcphead(buf, 1, 1, 0);

		// 需要用到MSS，所以采用4个字节的可选段
		SendTcp(buf, 4, TCP_FLAGS_SYNACK_V);

		return;
	}
	// 第三次同步应答,三次应答后方可传输数据
	if (tcp->Flags & TCP_FLAGS_ACK_V) // ACK确认标志位，为1表示此数据包为应答数据包
	{
		// 无数据返回ACK
		if (_net->PayloadLength == 0)
		{
			if (tcp->Flags & TCP_FLAGS_FIN_V)      //FIN结束连接请求标志位。为1表示是结束连接的请求数据包
			{
				//make_tcp_ack_from_any(buf, 0);
				make_tcphead(buf,1,0,1);
				SendTcp(buf, 0, TCP_FLAGS_ACK_V);
			}
			return;
		}
		///////////////////////////打印TCP数据/////////////////
		debug_printf("Data from TCP:");
		for(int i=0; i<_net->PayloadLength; i++)
			debug_printf("%c", _net->Payload[i]);

		debug_printf("\r\n");
		///////////////////////////////////////////////////////
		//make_tcp_ack_from_any(buf, _net->PayloadLength);       // 发送ACK，通知已收到
		make_tcphead(buf, _net->PayloadLength, 0, 1);
		SendTcp(buf, 0, TCP_FLAGS_ACK_V);

		TcpSend(buf, len);

		// tcp_close(buf,len);
		// for(;reset<BufferSize + 1;reset++)
		// 	buf[BufferSize + 1] = 0;
	}
}

void TinyIP::ProcessUdp(byte* buf, uint len)
{
	UDP_HEADER* udp = _net->UDP;

	Port = __REV16(udp->DestPort);
	RemotePort = __REV16(udp->SrcPort);

#if NET_DEBUG
	IP_HEADER* ip = _net->IP;
	debug_printf("UDP ");
	ShowIP(ip->SrcIP);
	debug_printf(":%d => ", __REV16(udp->SrcPort));
	ShowIP(ip->DestIP);
	debug_printf(":%d Payload=%d udp_len=%d \r\n", __REV16(udp->DestPort), _net->PayloadLength, __REV16(udp->Length));
#endif

	byte* data = _net->Payload;
	for(int i=0; i<_net->PayloadLength; i++)
	{
		debug_printf("%c", data[i]);
	}
	debug_printf("\r\n");

	udp->DestPort = udp->SrcPort;

	memcpy((byte*)(udp + sizeof(UDP_HEADER)), data, _net->PayloadLength);

	SendUdp(buf, _net->PayloadLength);
}

void TinyIP::ShowIP(byte* ip)
{
	debug_printf("%d", *ip++);
	for(int i=1; i<4; i++)
		debug_printf(".%d", *ip++);
}

void TinyIP::ShowMac(byte* mac)
{
	debug_printf("%02X", *mac++);
	for(int i=1; i<6; i++)
		debug_printf("-%02X", *mac++);
}

void TinyIP::SendEthernet(byte* buf, uint len)
{
	ETH_HEADER* eth = _net->Eth;
	assert_param(eth->Type == ETH_ARP || eth->Type == ETH_IP || eth->Type == ETH_IPv6);

	memcpy(&eth->DestMac, &RemoteMac, 6);
	memcpy(&eth->SrcMac, Mac, 6);

	len += sizeof(ETH_HEADER);
	if(len < 60) len = 60;	// 以太网最小包60字节

	//debug_printf("SendEthernet: %d\r\n", len);
	//ShowHex((byte*)eth, len);
	_enc->PacketSend((byte*)eth, len);
}

void TinyIP::SendIP(byte* buf, uint len)
{
	IP_HEADER* ip = _net->IP;
	assert_param(ip->Protocol == IP_ICMP ||
				 ip->Protocol == IP_IGMP ||
				 ip->Protocol == IP_TCP ||
				 ip->Protocol == IP_UDP);

	memcpy(&ip->DestIP, RemoteIP, 4);
	memcpy(&ip->SrcIP, IP, 4);

	ip->Version = 4;
	ip->Length = sizeof(IP_HEADER) / 4;
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + len);
	ip->Flags = 0x40;
	ip->FragmentOffset = 0;
	ip->TTL = 64;

	// 网络序是大端
	ip->Checksum = 0;
	ip->Checksum = __REV16((ushort)CheckSum((byte*)ip, sizeof(IP_HEADER), 0));

	_net->Eth->Type = ETH_IP;
	SendEthernet(buf, sizeof(IP_HEADER) + len);
}

void TinyIP::SendTcp(byte* buf, uint len, byte flags)
{
	TCP_HEADER* tcp = _net->TCP;

	tcp->SrcPort = __REV16(Port);
	tcp->DestPort = __REV16(RemotePort);
    tcp->Flags = flags;

	// 网络序是大端
	tcp->Checksum = 0;
	tcp->Checksum = __REV16((ushort)CheckSum((byte*)tcp - 8, 8 + sizeof(TCP_HEADER) + len, 2));

	_net->IP->Protocol = IP_TCP;
	SendIP(buf, sizeof(TCP_HEADER) + len);
}

void TinyIP::SendUdp(byte* buf, uint len, bool checksum)
{
	UDP_HEADER* udp = _net->UDP;

	udp->SrcPort = __REV16(Port);
	udp->DestPort = __REV16(RemotePort);
	udp->Length = __REV16(sizeof(UDP_HEADER) + len);

	// 网络序是大端
	udp->Checksum = 0;
	if(checksum) udp->Checksum = __REV16((ushort)CheckSum((byte*)udp, sizeof(UDP_HEADER) + len, 1));

	_net->IP->Protocol = IP_UDP;
	SendIP(buf, sizeof(UDP_HEADER) + len);
}

void TinyIP::SendDhcp(byte* buf, uint len)
{
	UDP_HEADER* udp = _net->UDP;
	DHCP_HEADER* dhcp = (DHCP_HEADER*)((byte*)udp + sizeof(UDP_HEADER));

	byte* p = (byte*)dhcp + sizeof(DHCP_HEADER);
	if(p[len - 1] != DHCP_OPT_End)
	{
		DHCP_OPT* opt = (DHCP_OPT*)(p + len);
		opt = opt->Next()->SetClientId(Mac, 6);
		opt = opt->Next()->SetData(DHCP_OPT_RequestedIP, IP, 4);
		opt = opt->Next()->SetData(DHCP_OPT_HostName, (byte*)"YWS SmartOS", 11);
		opt = opt->Next()->SetData(DHCP_OPT_Vendor, (byte*)"http://www.NewLifeX.com", 23);
		byte ps[] = { 0x01, 0x06, 0x03, 0x2b}; // 需要参数 Mask/DNS/Router/Vendor
		opt = opt->Next()->SetData(DHCP_OPT_ParameterList, ps, ArrayLength(ps));
		opt = opt->End();

		len = (byte*)opt + 1 - p;
	}

	dhcp->MsgType = 1;
	dhcp->HardType = 1;
	dhcp->HardLength = 6;
	dhcp->Hops = 0;
	dhcp->TransID = __REV(dhcp_id);
	dhcp->Flags = 0x80;	// 从0-15bits，最左一bit为1时表示server将以广播方式传送封包给 client，其余尚未使用
	dhcp->SetMagic();

	memcpy(dhcp->ClientMac, Mac, 6);

	// 如果最后一个字节不是DHCP_OPT_End，则增加End
	//byte* p = (byte*)dhcp + sizeof(DHCP_HEADER);
	//if(p[len - 1] != DHCP_OPT_End) p[len++] = DHCP_OPT_End;

	SendUdp(buf, sizeof(DHCP_HEADER) + len, false);
}

uint TinyIP::CheckSum(byte* buf, uint len, byte type)
{
    // type 0=ip
    //      1=udp
    //      2=tcp
    unsigned long sum = 0;

    if(type == 1)
    {
        sum+=IP_PROTO_UDP_V; // protocol udp
        // the length here is the length of udp (data+header len)
        // =length given to this function - (IP.scr+IP.dst length)
        sum+=len-8; // = real tcp len
    }
    if(type == 2)
    {
        sum+=IP_PROTO_TCP_V;
        // the length here is the length of tcp (data+header len)
        // =length given to this function - (IP.scr+IP.dst length)
        sum+=len-8; // = real tcp len
    }
    // build the sum of 16bit words
    while(len > 1)
    {
        sum += 0xFFFF & (*buf<<8|*(buf+1));
        buf += 2;
        len -= 2;
    }
    // if there is a byte left then add it (padded with zero)
    if (len)
    {
        sum += (0xFF & *buf)<<8;
    }
    // now calculate the sum over the bytes in the sum
    // until the result is only 16bit long
    while (sum>>16)
    {
        sum = (sum & 0xFFFF)+(sum >> 16);
    }
    // build 1's complement:
    return( (uint) sum ^ 0xFFFF);
}

void TinyIP::make_tcphead(byte* buf, uint rel_ack_num, byte mss, byte cp_seq)
{
	TCP_HEADER* tcp = _net->TCP;

	/*ushort port = tcp->SrcPort;
	tcp->SrcPort = tcp->DestPort;
	tcp->DestPort = port;*/

    byte i = 4;
    // sequence numbers:
    // add the rel ack num to SEQACK
    while(i>0)
    {
        rel_ack_num = buf[TCP_SEQ_H_P + i-1] + rel_ack_num;
        byte tseq = buf[TCP_SEQACK_H_P + i-1];
        buf[TCP_SEQACK_H_P + i-1] = 0xff & rel_ack_num;
        if (cp_seq)
        {
            // copy the acknum sent to us into the sequence number
            buf[TCP_SEQ_H_P + i-1] = tseq;
        }
        else
        {
            buf[TCP_SEQ_H_P + i-1] = 0; // some preset vallue
        }
        rel_ack_num = rel_ack_num >> 8;
        i--;
    }
    if (cp_seq == 0)
    {
		// 我们仅仅递增第二个字节，这将允许我们以256或者512字节来发包
		tcp->Seq = __REV(seqnum << 8);
        // step the inititial seq num by something we will not use
        // during this tcp session:
        seqnum += 2;
    }
	//tcp->Checksum = 0;

	tcp->Length = sizeof(TCP_HEADER);
    // 头部后面可能有可选数据，Length决定头部总长度（4的倍数）
    if (mss)
    {
        // 使用可选域设置 MSS 到 1408:0x580
		uint p = sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(TCP_HEADER);
		*(uint *)(buf + p) = __REV(0x02040580);

		tcp->Length++;
    }
}

/*void TinyIP::make_tcp_ack_from_any(byte* buf, uint dlen)
{
    if (dlen == 0)
    {
        // if there is no data then we must still acknoledge one packet
        make_tcphead(buf,1,0,1); // no options
    }
    else
    {
        make_tcphead(buf, dlen, 0, 1); // no options
    }

	SendTcp(buf, 0, TCP_FLAGS_ACK_V);
}*/

void TinyIP::make_tcp_ack_with_data(byte* buf, uint dlen)
{
	SendTcp(buf, dlen, TCP_FLAGS_ACK_V | TCP_FLAGS_PUSH_V /*| TCP_FLAGS_FIN_V*/);
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
void TinyIP::dhcp_discover()
{
	byte* buf = Buffer;
	DHCP_HEADER* dhcp = (DHCP_HEADER*)((byte*)_net->UDP + sizeof(UDP_HEADER));

	byte* p = (byte*)dhcp + sizeof(DHCP_HEADER);
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Discover);

	SendDhcp(buf, (byte*)opt->Next() - p);
}

void TinyIP::dhcp_request(byte* buf)
{
	DHCP_HEADER* dhcp = (DHCP_HEADER*)((byte*)_net->UDP + sizeof(UDP_HEADER));

	byte* p = (byte*)dhcp + sizeof(DHCP_HEADER);
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Request);
	opt = opt->Next()->SetData(DHCP_OPT_DHCPServer, DHCPServer, 4);

	SendDhcp(buf, (byte*)opt->Next() - p);
}

void TinyIP::PareOption(byte* buf, int len)
{
	UDP_HEADER* udp = _net->UDP;
	DHCP_HEADER* dhcp = (DHCP_HEADER*)((byte*)udp + sizeof(UDP_HEADER));
	byte* p = (byte*)dhcp + sizeof(DHCP_HEADER);
	byte* end = p + len;
	while(p < end)
	{
		byte opt = *p++;
		if(opt == DHCP_OPT_End) break;
		byte len = *p++;
		switch(opt)
		{
			case DHCP_OPT_Mask: memcpy(Mask, p, len); break;
			case DHCP_OPT_DNSServer: memcpy(DNSServer, p, len); break;
			case DHCP_OPT_Router: memcpy(Gateway, p, len); break;
			case DHCP_OPT_DHCPServer: memcpy(DHCPServer, p, len); break;
#if NET_DEBUG
			//default:
			//	debug_printf("Unkown DHCP Option=%d Length=%d\r\n", opt, len);
#endif
		}
		p += len;
	}
}

void TinyIP::DHCPConfig(byte* buf)
{
	// 先设置数据包，下面马上要用到
	_net->SetUDP();
	memset(RemoteMac, 0xFF, 6);
	//memset(IP, 0x00, 6);
	memset(RemoteIP, 0xFF, 6);
	Port = 68;
	RemotePort = 67;

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
			dhcp_discover();
			
			next = Time.NewTicks(1 * 1000000);
		}

		uint len = _enc->PacketReceive(buf, BufferSize);
        // 如果缓冲器里面没有数据则转入下一次循环
        if(!_net->Unpack(len)) continue;

		IP_HEADER* ip = _net->IP;
		if(!ip) continue;

		UDP_HEADER* udp = _net->UDP;
		if(!udp || __REV16(udp->DestPort) != 68) continue;

		// DHCP附加数据的长度
		len = _net->PayloadLength;
		if(len <= sizeof(DHCP_HEADER)) continue;

		len -= sizeof(DHCP_HEADER);

		DHCP_HEADER* dhcp = (DHCP_HEADER*)((byte*)udp + sizeof(UDP_HEADER));
		if(!dhcp->Valid())continue;

		// 获取DHCP消息类型
		DHCP_OPT* opt = GetOption((byte*)dhcp + sizeof(DHCP_HEADER), len, DHCP_OPT_MessageType);
		if(!opt) continue;

		if(opt->Data == DHCP_TYPE_Offer)
		{
			if(__REV(dhcp->TransID) == dhcp_id)
			{
				memcpy(IP, dhcp->YourIP, 4);
				PareOption(buf, _net->PayloadLength - sizeof(DHCP_HEADER));

				// 向网络宣告已经确认使用哪一个DHCP服务器提供的IP地址
				// 这里其实还应该发送ARP包确认IP是否被占用，如果被占用，还需要拒绝服务器提供的IP，比较复杂，可能性很低，暂时不考虑
				debug_printf("DHCP Request  IP...\r\n");
				dhcp_request(buf);
			}
		}
		else if(opt->Data == DHCP_TYPE_Ack)
		{
			debug_printf("DHCP Response IP:");
			ShowIP(dhcp->YourIP);
			debug_printf("\r\n");

			if(memcmp(dhcp->YourIP, IP, 4) == 0)
			{
				IPIsReady = true;
				break;
			}
		}
		else if(opt->Data == DHCP_TYPE_Nak)
		{
			opt = GetOption((byte*)dhcp + sizeof(DHCP_HEADER), len, DHCP_OPT_Message);
			debug_printf("DHCP Nak IP:");
			ShowIP(IP);
			if(opt)
			{
				debug_printf(" ");
				byte* str = &opt->Data;
				for(int i=0; i<opt->Length; i++)
					debug_printf("%c", str[i]);
			}
			debug_printf("\r\n");
		}
		else
			debug_printf("DHCP Unkown Type=%d\r\n", opt->Data);
	}
}

void TinyIP::Send(byte* buf, uint len)
{
}
