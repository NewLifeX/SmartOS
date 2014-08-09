#include "TinyIP.h"
#include "../Net/Net.h"

#define NET_DEBUG DEBUG

TinyIP::TinyIP(Enc28j60* enc, byte ip[4], byte mac[6])
{
	_enc = enc;
	memcpy(IP, ip, 4);
	memcpy(Mac, mac, 6);

	Buffer = NULL;
	BufferSize = 1500;

	seqnum = 0xa;
}

TinyIP::~TinyIP()
{
    _enc = NULL;
	if(Buffer) delete Buffer;
}

void TinyIP::TcpClose(byte* buf, uint size)
{
	IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	TCP_HEADER* tcp = (TCP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));
    // fill the header:
    // This code requires that we send only one data packet
    // because we keep no state information. We must therefore set
    // the fin here:
    tcp->Flags = TCP_FLAGS_ACK_V | TCP_FLAGS_PUSH_V | TCP_FLAGS_FIN_V;

    // total length field in the IP header must be set:
    // 20 bytes IP + 20 bytes tcp (when no options) + len of data
    //uint j = IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + size;
    //buf[IP_TOTLEN_H_P] = j >> 8;
    //buf[IP_TOTLEN_L_P] = j & 0xff;
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + sizeof(TCP_HEADER) + size);
    fill_ip_hdr_checksum(buf);
    // zero the checksum
    /*buf[TCP_CHECKSUM_H_P] = 0;
    buf[TCP_CHECKSUM_L_P] = 0;
    // calculate the checksum, len=8 (start from ip.src)  +  TCP_HEADER_LEN_PLAIN  +  data len
    uint j = checksum(&buf[IP_SRC_P], 8 + TCP_HEADER_LEN_PLAIN + size,2);
    buf[TCP_CHECKSUM_H_P] = j >> 8;
    buf[TCP_CHECKSUM_L_P] = j & 0xff;
    _enc->PacketSend(buf, IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + size + ETH_HEADER_LEN);*/
	SendTcp(buf, size);
}

void TinyIP::TcpSend(byte* buf, uint size)
{
	IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	TCP_HEADER* tcp = (TCP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));
    // fill the header:
    // This code requires that we send only one data packet
    // because we keep no state information. We must therefore set
    // the fin here:
    tcp->Flags = TCP_FLAGS_ACK_V | TCP_FLAGS_PUSH_V;

    // total length field in the IP header must be set:
    // 20 bytes IP  +  20 bytes tcp (when no options)  +  len of data
    //uint j = IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + size;
    //buf[IP_TOTLEN_H_P] = j >> 8;
    //buf[IP_TOTLEN_L_P] = j & 0xff;
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + sizeof(TCP_HEADER) + size);
    fill_ip_hdr_checksum(buf);
    // zero the checksum
    /*buf[TCP_CHECKSUM_H_P] = 0;
    buf[TCP_CHECKSUM_L_P] = 0;
    // calculate the checksum, len=8 (start from ip.src)  +  TCP_HEADER_LEN_PLAIN  +  data len
    j = checksum(&buf[IP_SRC_P], 8 + TCP_HEADER_LEN_PLAIN + size, 2);
    buf[TCP_CHECKSUM_H_P] = j >> 8;
    buf[TCP_CHECKSUM_L_P] = j & 0xff;
		//debug_printf("len=%d\r\n",tcp_d_len);
    _enc->PacketSend(buf, IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + size + ETH_HEADER_LEN);*/
	SendTcp(buf, size);
}

void TinyIP::Start()
{
	// 分配缓冲区。比较大，小心栈空间不够
	if(!Buffer) Buffer = new byte[BufferSize + 1];
	assert_param(Buffer);
	assert_param(Sys.CheckMemory());
	byte* buf = Buffer;

    // 初始化 enc28j60 的MAC地址(物理地址),这个函数必须要调用一次
    _enc->Init((string)Mac);

    // 将enc28j60第三引脚的时钟输出改为：from 6.25MHz to 12.5MHz(本例程该引脚NC,没用到)
    _enc->ClockOut(2);

	if(UseDHCP)
	{
		IPIsReady = false;
		dhcp_id = (uint)g_Time->CurrentTicks();

		DHCP_config(buf);

		_enc->Init((string)Mac);
		_enc->ClockOut(2);
	}

	uint eth_size = sizeof(ETH_HEADER);
	//uint ip_size = eth_size + sizeof(IP_HEADER);
    while(1)
    {
        // 获取缓冲区的包
        uint len = _enc->PacketReceive(buf, BufferSize);

        // 如果缓冲器里面没有数据则转入下一次循环
        if(!len) continue;
		// 不够一帧
		if(len < eth_size) continue;

		ETH_HEADER* eth = (ETH_HEADER*)buf;
		// 处理ARP
		if(eth->Type == ETH_ARP)
		{
			ProcessArp(buf, len);
            continue;
		}
		
#if NET_DEBUG
		if(eth->Type != ETH_IP) debug_printf("Unkown EthernetType 0x%02X\r\n", eth->Type);
#endif

		IP_HEADER* ip = (IP_HEADER*)(buf + eth_size);
		// 是否发给本机。注意memcmp相等返回0
		if(memcmp(ip->DestIP, IP, 4) !=0 ) continue;

        if(ip->Protocol == IP_ICMP)
        {
#if NET_DEBUG
			debug_printf("Ping From "); // 打印发方的ip
			//ShowIP(&buf[IP_SRC_P]);
			ShowIP(ip->SrcIP);
			debug_printf("\r\n");
#endif

			ProcessICMP(buf, len);
            continue;
        }
        if (ip->Protocol == IP_TCP)
        {
			ProcessTcp(buf, len);
			continue;
        }
        if (ip->Protocol == IP_UDP /*&& buf[UDP_DST_PORT_H_P] == 4*/)
        {
			ProcessUdp(buf, len);
			continue;
        }

#if NET_DEBUG
		debug_printf("IP Unkown Protocol=%d ", ip->Protocol);
		ShowIP(ip->SrcIP);
		debug_printf(" => ");
		ShowIP(ip->DestIP);
		debug_printf("\r\n");
#endif
    }
}

void TinyIP::ProcessArp(byte* buf, uint len)
{
	// ARP头
	if(len < sizeof(ETH_HEADER) + sizeof(ARP_HEADER)) return;

	/*
	当封装的ARP报文在以太网上传输时，硬件类型字段赋值为0x0100，标识硬件为以太网硬件；
	协议类型字段赋值为0x0800，标识上次协议为IP协议；由于以太网的MAC地址为48比特位，IP地址为32比特位，则硬件地址长度字段赋值为6，协议地址长度字段赋值为4 ；
	选项字段标识ARP报文的类型，当为请求报文时，赋值为0x0100，当为回答报文时，赋值为0x0200。
	*/
	ARP_HEADER* arp = (ARP_HEADER*)(buf + sizeof(ETH_HEADER));

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
	debug_printf("]\r\n");
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
	debug_printf("\r\n");
#endif

	SendEthernet(buf, sizeof(ARP_HEADER));
}

void TinyIP::ProcessICMP(byte* buf, uint len)
{
	len -= sizeof(ETH_HEADER) + sizeof(IP_HEADER);
	if(len < sizeof(ICMP_HEADER)) return;

	ICMP_HEADER* icmp = (ICMP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));
	// 只处理ECHO请求
	if(icmp->Type != 8) return;
	
	icmp->Type = 0; // 响应
	// 因为仅仅改变类型，因此我们能够提前修正校验码
	icmp->Checksum += 0x08;

	// 这里不能直接用sizeof(ICMP_HEADER)，而必须用len，因为ICMP包后面一般有附加数据
    SendIP(buf, len);
}

void TinyIP::ProcessTcp(byte* buf, uint len)
{
	len -= sizeof(ETH_HEADER) + sizeof(IP_HEADER);
	if(len < sizeof(TCP_HEADER)) return;

	IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	TCP_HEADER* tcp = (TCP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));

#if NET_DEBUG
	debug_printf("TCP ");
	ShowIP(ip->SrcIP);
	debug_printf(":%d => ", __REV16(tcp->SrcPort));
	ShowIP(ip->DestIP);
	debug_printf(":%d\r\n", __REV16(tcp->DestPort));
#endif

	// 第一次同步应答
	if (tcp->Flags & TCP_FLAGS_SYN_V) // SYN连接请求标志位，为1表示发起连接的请求数据包
	{
		debug_printf("\tRequest From "); // 打印发送方的ip
		ShowIP(ip->SrcIP);
		debug_printf("\r\n");

		//第二次同步应答
		//make_tcp_synack_from_syn(buf);

		//uint ck;
		//make_eth(buf);
		// total length field in the IP header must be set:
		// 20 bytes IP + 24 bytes (20tcp+4tcp options)
		//buf[IP_TOTLEN_H_P] = 0;
		//buf[IP_TOTLEN_L_P] = IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+4;
		//make_ip(buf);
		ip->TotalLength = __REV16(sizeof(IP_HEADER) + sizeof(TCP_HEADER) + 4);
		tcp->Flags = TCP_FLAGS_SYNACK_V;
		make_tcphead(buf, 1, 1, 0);
		// calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + 4 (one option: mss)
		/*ck = checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN+4,2);
		buf[TCP_CHECKSUM_H_P] = ck >> 8;
		buf[TCP_CHECKSUM_L_P] = ck & 0xff;
		// add 4 for option mss:
		_enc->PacketSend(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+4+ETH_HEADER_LEN);*/
		SendTcp(buf, 4);

		return;
	}
	// 第三次同步应答,三次应答后方可传输数据
	if (tcp->Flags & TCP_FLAGS_ACK_V) // ACK确认标志位，为1表示此数据包为应答数据包
	{
		//IP包长度
		uint dlen = (buf[IP_TOTLEN_H_P]<<8)|(buf[IP_TOTLEN_L_P]&0xff);
		//减去IP首部长度
		dlen -= IP_HEADER_LEN;
		//TCP首部长度，因为TCP协议规定了只有四位来表示长度，所以需要以下处理,4*6=24
		uint info_hdr_len = (buf[TCP_HEADER_LEN_P]>>4)*4; // generate len in bytes;
		//减去TCP首部长度
		dlen -= info_hdr_len;
		
		uint dat_p = 0;
		if(dlen) dat_p = (uint)TCP_SRC_PORT_H_P + info_hdr_len;

		//无数据返回ACK
		if (dat_p == 0)
		{
			if (tcp->Flags & TCP_FLAGS_FIN_V)      //FIN结束连接请求标志位。为1表示是结束连接的请求数据包
			{
				make_tcp_ack_from_any(buf, dlen);
			}
			return;
		}
		///////////////////////////打印TCP数据/////////////////
		debug_printf("Data from TCP:");
		uint i = 0;
		while(i < dlen)
		{
			debug_printf("%c", buf[dat_p + i]);
			i++;
		}
		debug_printf("\r\n");
		///////////////////////////////////////////////////////
		make_tcp_ack_from_any(buf, dlen);       // 发送ACK，通知已收到
		TcpSend(buf, len);

		// tcp_close(buf,len);
		// for(;reset<BufferSize + 1;reset++)
		// 	buf[BufferSize + 1] = 0;
	}
}

void TinyIP::ProcessUdp(byte* buf, uint len)
{
	IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	UDP_HEADER* udp = (UDP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));

#if NET_DEBUG
	debug_printf("UDP ");
	ShowIP(ip->SrcIP);
	debug_printf(":%d => ", __REV16(udp->SrcPort));
	ShowIP(ip->DestIP);
	debug_printf(":%d\r\n", __REV16(udp->DestPort));
#endif

	//UDP数据长度
	uint datalen = buf[UDP_LEN_H_P];
	datalen = datalen << 8;
	datalen = (datalen + buf[UDP_LEN_L_P]) - sizeof(UDP_HEADER);

	byte* data = new byte[datalen];
	for(int i=0; i<datalen; i++)
	{
		data[i] = buf[UDP_DATA_P + i];
		debug_printf("%c", data[i]);
	}
	debug_printf("\r\n");

	//获取发送源端口
	//ushort port = buf[UDP_SRC_PORT_H_P] << 8 | buf[UDP_SRC_PORT_L_P];

	// 最大发送220字节数据
    //make_eth(buf);
    //if (datalen>220)
    //	{
    //    datalen=220;
    //	}

    // total length field in the IP header must be set:
    //uint i = IP_HEADER_LEN + UDP_HEADER_LEN + datalen;
    //buf[IP_TOTLEN_H_P] = i >> 8;
    //buf[IP_TOTLEN_L_P] = i;
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + sizeof(UDP_HEADER) + datalen);
    make_ip(buf);
    //buf[UDP_DST_PORT_H_P] = port >> 8;
    //buf[UDP_DST_PORT_L_P] = port & 0xff;
	udp->DestPort = udp->SrcPort;
    // source port does not matter and is what the sender used.
    // calculte the udp length:
    //buf[UDP_LEN_H_P] = datalen >> 8;
    //buf[UDP_LEN_L_P] = UDP_HEADER_LEN+datalen;
	udp->Length = sizeof(UDP_HEADER) + datalen;
    // zero the checksum
    /*buf[UDP_CHECKSUM_H_P] = 0;
    buf[UDP_CHECKSUM_L_P] = 0;
    // copy the data:
    while(i<datalen)
    {
        buf[UDP_DATA_P + i] = data[i];
        i++;
    }
	//这里的16字节是UDP的伪首部，即IP的源地址-0x1a+目标地址-0x1e
	//+UDP首部=4+4+8=16
    uint ck=checksum(&buf[IP_SRC_P], 16 + datalen,1);
    buf[UDP_CHECKSUM_H_P] = ck >> 8;
    buf[UDP_CHECKSUM_L_P] = ck & 0xff;
    _enc->PacketSend(buf, UDP_HEADER_LEN+IP_HEADER_LEN+ETH_HEADER_LEN+datalen);*/
	
	memcpy((byte*)(udp + sizeof(UDP_HEADER)), data, datalen);

	SendUdp(buf, datalen);
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
	ETH_HEADER* eth = (ETH_HEADER*)buf;
	memcpy(&eth->DestMac, &eth->SrcMac, 6);
	memcpy(&eth->SrcMac, Mac, 6);

	_enc->PacketSend(buf, sizeof(ETH_HEADER) + len);
}

void TinyIP::SendIP(byte* buf, uint len)
{
	IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	memcpy(&ip->DestIP, &ip->SrcIP, 4);
	memcpy(&ip->SrcIP, IP, 4);

	ip->Checksum = 0;
	ip->Flags = 0x40;
	ip->FragmentOffset = 0;
	ip->TTL = 64;

	// 网络序是大端
	ip->Checksum = __REV16((ushort)checksum(buf + sizeof(ETH_HEADER), sizeof(IP_HEADER), 0));

	SendEthernet(buf, sizeof(IP_HEADER) + len);
}

void TinyIP::SendTcp(byte* buf, uint len)
{
	TCP_HEADER* tcp = (TCP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));

	tcp->Checksum = 0;
	// 网络序是大端
	tcp->Checksum = __REV16((ushort)checksum(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER) - 8, 8 + sizeof(TCP_HEADER) + len, 2));

	SendIP(buf, sizeof(TCP_HEADER) + len);
}

void TinyIP::SendUdp(byte* buf, uint len)
{
	UDP_HEADER* udp = (UDP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));

	udp->Checksum = 0;
	// 网络序是大端
	udp->Checksum = __REV16((ushort)checksum(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER), sizeof(UDP_HEADER) + len, 1));

	SendIP(buf, sizeof(UDP_HEADER) + len);
}

uint TinyIP::checksum(byte* buf, uint len, byte type)
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

// make a return eth header from a received eth packet
void TinyIP::make_eth(byte* buf)
{
    //copy the destination mac from the source and fill my mac into src
    /*for(int i=0; i<6; i++)
    {
        buf[ETH_DST_MAC +i] = buf[ETH_SRC_MAC +i];
        buf[ETH_SRC_MAC +i] = Mac[i];
    }*/

	ETH_HEADER* eth = (ETH_HEADER*)buf;
	memcpy(&eth->DestMac, &eth->SrcMac, 6);
	memcpy(&eth->SrcMac, Mac, 6);
}

void TinyIP::fill_ip_hdr_checksum(byte* buf)
{
    /*uint ck;
    // clear the 2 byte checksum
    buf[IP_CHECKSUM_P] = 0;
    buf[IP_CHECKSUM_P + 1] = 0;
    buf[IP_FLAGS_P] = 0x40; // don't fragment
    buf[IP_FLAGS_P + 1] = 0;  // fragement offset
    buf[IP_TTL_P] = 64; // ttl*/

	IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	ip->Checksum = 0;
	ip->Flags = 0x40;
	ip->FragmentOffset = 0;
	ip->TTL = 64;

    // calculate the checksum:
    /*uint ck = checksum(&buf[IP_P], IP_HEADER_LEN, 0);
    buf[IP_CHECKSUM_P] = ck >> 8;
    buf[IP_CHECKSUM_P + 1] = ck & 0xff;*/

	// 网络序是大端
	ip->Checksum = __REV16((ushort)checksum(buf + sizeof(ETH_HEADER), sizeof(IP_HEADER), 0));
}

// make a return ip header from a received ip packet
void TinyIP::make_ip(byte* buf)
{
    for(int i=0; i<4; i++)
    {
        buf[IP_DST_P + i] = buf[IP_SRC_P + i];
        buf[IP_SRC_P + i] = IP[i];
    }
    fill_ip_hdr_checksum(buf);
}

// make a return tcp header from a received tcp packet
// rel_ack_num is how much we must step the seq number received from the
// other side. We do not send more than 255 bytes of text (=data) in the tcp packet.
// If mss=1 then mss is included in the options list
//
// After calling this function you can fill in the first data byte at TCP_OPTIONS_P + 4
// If cp_seq=0 then an initial sequence number is used (should be use in synack)
// otherwise it is copied from the packet we received
void TinyIP::make_tcphead(byte* buf, uint rel_ack_num, byte mss, byte cp_seq)
{
	//IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	TCP_HEADER* tcp = (TCP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));

    /*byte i=0;
    byte tseq;
    while(i<2)
    {
        buf[TCP_DST_PORT_H_P + i] = buf[TCP_SRC_PORT_H_P + i];
        buf[TCP_SRC_PORT_H_P + i] = 0; // clear source port
        i++;
    }
	buf[TCP_SRC_PORT_H_P] = RemotePort >> 8;
	buf[TCP_SRC_PORT_L_P] = RemotePort & 0xFF;*/
	ushort port = tcp->SrcPort;
	tcp->SrcPort = tcp->DestPort;
	tcp->DestPort = port;
	//tcp->SrcPort = __REV16(RemotePort);

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
        // put inital seq number
        /*buf[TCP_SEQ_H_P + 0] = 0;
        buf[TCP_SEQ_H_P + 1] = 0;
        // we step only the second byte, this allows us to send packts
        // with 255 bytes or 512 (if we step the initial seqnum by 2)
        buf[TCP_SEQ_H_P + 2] = seqnum;
        buf[TCP_SEQ_H_P + 3] = 0;*/
		// 我们仅仅递增第二个字节，这将允许我们以256或者512字节来发包
		tcp->Seq = __REV(seqnum << 8);
        // step the inititial seq num by something we will not use
        // during this tcp session:
        seqnum += 2;
    }
    // zero the checksum
    //buf[TCP_CHECKSUM_H_P] = 0;
    //buf[TCP_CHECKSUM_L_P] = 0;
	tcp->Checksum = 0;

	tcp->Length = sizeof(TCP_HEADER);
    // 头部后面可能有可选数据，Length决定头部总长度（4的倍数）
    if (mss)
    {
        // 使用可选域设置 MSS 到 1408:0x580
		uint p = sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(TCP_HEADER);
		*(uint *)(buf + p) = __REV(0x02040580);
        //buf[TCP_OPTIONS_P] = 2;
        //buf[TCP_OPTIONS_P + 1] = 4;
        //buf[TCP_OPTIONS_P + 2] = 0x05;
        //buf[TCP_OPTIONS_P + 3] = 0x80;
        // 24 bytes:
        //buf[TCP_HEADER_LEN_P] = 0x60;

		tcp->Length++;
    }
}

void TinyIP::make_tcp_synack_from_syn(byte* buf)
{
	IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	TCP_HEADER* tcp = (TCP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));

    //uint ck;
    //make_eth(buf);
    // total length field in the IP header must be set:
    // 20 bytes IP + 24 bytes (20tcp+4tcp options)
    //buf[IP_TOTLEN_H_P] = 0;
    //buf[IP_TOTLEN_L_P] = IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+4;
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + sizeof(TCP_HEADER) + 4);
    make_ip(buf);
    tcp->Flags = TCP_FLAGS_SYNACK_V;
    make_tcphead(buf,1,1,0);
    // calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + 4 (one option: mss)
    /*ck=checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN+4,2);
    buf[TCP_CHECKSUM_H_P] = ck >> 8;
    buf[TCP_CHECKSUM_L_P] = ck & 0xff;
    // add 4 for option mss:
    _enc->PacketSend(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+4+ETH_HEADER_LEN);*/
	SendTcp(buf, 4);
}

// fill in tcp data at position pos. pos=0 means start of
// tcp data. Returns the position at which the string after
// this string could be filled.
uint TinyIP::fill_tcp_data_p(byte* buf, uint pos, const byte* progmem_s)
{
    byte c;
    // fill in tcp data at position pos
    //
    // with no options the data starts after the checksum + 2 more bytes (urgent ptr)
    while (*progmem_s)
    {
		c = *progmem_s++;
        buf[TCP_CHECKSUM_L_P + 3 + pos] = c;
        pos++;
    }
    return(pos);
}

// fill in tcp data at position pos. pos=0 means start of
// tcp data. Returns the position at which the string after
// this string could be filled.
uint TinyIP::fill_tcp_data(byte* buf, uint pos, const byte* s)
{
    // fill in tcp data at position pos
    //
    // with no options the data starts after the checksum + 2 more bytes (urgent ptr)
    while (*s)
    {
        buf[TCP_CHECKSUM_L_P + 3+pos]=*s;
        pos++;
        s++;
    }
    return(pos);
}

// Make just an ack packet with no tcp data inside
// This will modify the eth/ip/tcp header
void TinyIP::make_tcp_ack_from_any(byte* buf, uint dlen)
{
	IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	TCP_HEADER* tcp = (TCP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));

    //uint j;
    //make_eth(buf);
    // fill the header:
    tcp->Flags = TCP_FLAGS_ACK_V;

    if (dlen == 0)
    {
        // if there is no data then we must still acknoledge one packet
        make_tcphead(buf,1,0,1); // no options
    }
    else
    {
        make_tcphead(buf, dlen, 0, 1); // no options
    }

    // total length field in the IP header must be set:
    // 20 bytes IP + 20 bytes tcp (when no options)
    //j=IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN;
    //buf[IP_TOTLEN_H_P] = j >> 8;
    //buf[IP_TOTLEN_L_P] = j& 0xff;
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + sizeof(TCP_HEADER));
    make_ip(buf);
    // calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + data len
    /*j=checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN,2);
    buf[TCP_CHECKSUM_H_P] = j >> 8;
    buf[TCP_CHECKSUM_L_P] = j& 0xff;
    _enc->PacketSend(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+ETH_HEADER_LEN);*/
	SendTcp(buf, 0);
}

// dlen is the amount of tcp data (http data) we send in this packet
// You can use this function only immediately after make_tcp_ack_from_any
// This is because this function will NOT modify the eth/ip/tcp header except for
// length and checksum
void TinyIP::make_tcp_ack_with_data(byte* buf, uint dlen)
{
	IP_HEADER* ip = (IP_HEADER*)(buf + sizeof(ETH_HEADER));
	TCP_HEADER* tcp = (TCP_HEADER*)(buf + sizeof(ETH_HEADER) + sizeof(IP_HEADER));

    //uint j;
    // fill the header:
    // This code requires that we send only one data packet
    // because we keep no state information. We must therefore set
    // the fin here:
    tcp->Flags = TCP_FLAGS_ACK_V|TCP_FLAGS_PUSH_V;//|TCP_FLAGS_FIN_V;

    // total length field in the IP header must be set:
    // 20 bytes IP + 20 bytes tcp (when no options) + len of data
    //j=IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+dlen;
    //buf[IP_TOTLEN_H_P] = j >> 8;
    //buf[IP_TOTLEN_L_P] = j& 0xff;
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + sizeof(TCP_HEADER));
    fill_ip_hdr_checksum(buf);
    // zero the checksum
    /*buf[TCP_CHECKSUM_H_P] = 0;
    buf[TCP_CHECKSUM_L_P] = 0;
    // calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + data len
    j=checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN+dlen,2);
    buf[TCP_CHECKSUM_H_P] = j >> 8;
    buf[TCP_CHECKSUM_L_P] = j& 0xff;
    _enc->PacketSend(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+dlen+ETH_HEADER_LEN);*/
	SendTcp(buf, 0);
}

unsigned char hex_to_dec_L(int d)
{
	unsigned char b[2];
	b[1]=d%16;     //1位
	b[0]=d/16%16;  //2位
	return (unsigned char)((b[0]<<4)+b[1]);
}

unsigned char hex_to_dec_H(int d)
{
	unsigned char b[4];
	b[3]=d%16;     //1位
	b[2]=d/16%16;  //2位
	b[1]=d/16/16%16;  //3位
	b[0]=d/16/16/16%16;  //4位
	if((d/16/16/16%16) == 0)
		b[0]=d/16/16/16;
	return (unsigned char)((b[0]<<4)+b[1]);
}

void TinyIP::dhcp_discover(byte* buf)
{
	dhcp_fill_public_data(buf);
	//	char i=0;
	//	for(i=0;i<6;i++) //填充以太网头部mac	//
	//	{  				//
	//		buf[ETH_DST_MAC+i] = 0xff;
	//		buf[ETH_SRC_MAC+i]=mymac[i];
	//		buf[0x46+i]=mymac[i]; //client mac
	//		buf[0x120+i]=mymac[i]; //option client mac
	//	}

	buf[0x0c] = 0x08; // 0x80 0x00  ip包
	//	buf[0x0d] = 0x00;

	//	buf[0x0e] = 0x45; //4代表 ipv4 5代表5*4bytes=20 bytes ipv4头部长度
	//	buf[0x0f] = 0x00;

	buf[0x10]=hex_to_dec_H(0x143-0xe); //长度为328 bytes
	buf[0x11]=hex_to_dec_L(0x143-0xe);

	//	buf[0x14] = 0x00;     //                        //fragment offset =0
	//	buf[0x15] = 0x00;//

	//	buf[0x16] = 0x40;			//												//ttl=64
	//	buf[0x17] = 0x11;			//												//udp协议

	fill_ip_hdr_checksum(buf);

	//	for(i=0;i<4;i++)								//					  //填充ip
	//	{//
	//		buf[IP_SRC_P+i] = 0x00;//
	//		buf[IP_DST_P+i] = 0xff;//
	//	}//

	//  buf[0x22] = 0x00;//
	//	buf[0x23] = 0x44;           //                  //本地dhcp端口为68

	//	buf[0x24] = 0x00;//
	//	buf[0x25] = 0x43;					//										//dhcp服务器端口

	buf[0x26]=hex_to_dec_H(0x143-0xe-0x14);
	buf[0x27]=hex_to_dec_L(0x143-0xe-0x14); 	//长度=udp+bootstrap

	//	buf[0x28]=
	//	buf[0x29]= 			//udp checksum

	//	buf[0x2a] = 0x01;                 //            //boot request
	//	buf[0x2b] = 0x01;									//						//硬件类型  ethernet
	//
	//	buf[0x2c] = 0x06;									//						//硬件地址长度 6
	//	buf[0x2d] = 0x00; 	//Hops 每站加1

	//	buf[0x2e]=dhcp_id>>24;			//					//dhcp识别码
	//	buf[0x2f]=dhcp_id>>16&0xff;//
	//	buf[0x30]=dhcp_id>>8&0xff;//
	//	buf[0x31]=dhcp_id&0xff;	//

	//	buf[0x34] = 0x80;// 	//最左一bit为1时表示server将以广播方式传递封包给client，其余尚未使用

	//	buf[0x116] = 0x63; //DHCP
	//	buf[0x117] = 0x82;
	//	buf[0x118] = 0x53;
	//	buf[0x119] = 0x63;

	buf[0x11a] = 0x35; //option DHCP message type
	buf[0x11b] = 0x01; //lenth=1
	buf[0x11c] = 0x01; //discover=1

	buf[0x11d] = 0x3d; //option client identifier
	buf[0x11e] = 0x07; //option 长度  1一个mac+一字节类型
	buf[0x11f] = 0x01; //类型ETHERNET=1

	buf[0x126] = 0x32; //requested ip option
	buf[0x127] = 0x04; //option lenth

	buf[0x128] = 0x00; //请求的ip
	buf[0x129] = 0x00;
	buf[0x12a] = 0x00;
	buf[0x12b] = 0x00;

	buf[0x12c] = 0x0c; //host option
	buf[0x12d] = 0x04; //长度为4
	buf[0x12e] = 0x61; //字符 a
	buf[0x12f] = 0x62; //字符 b
	buf[0x130] = 0x63; //字符 c
	buf[0x131] = 0x64; //字符 d

	buf[0x132] = 0x3c; //vendor option
	buf[0x133] = 0x08; //长度为8
	buf[0x134] = 0x4d; //下面为vender信息
	buf[0x135] = 0x53;
	buf[0x136] = 0x46;
	buf[0x137] = 0x54;
	buf[0x138] = 0x20;
	buf[0x139] = 0x35;
	buf[0x13a] = 0x2e;
	buf[0x13b] = 0x30;

	buf[0x13c] = 0x37; //请求列表
	buf[0x13d] = 0x04; //长度
	buf[0x13e] = 0x01; //mask
	buf[0x13f] = 0x06; //dns
	buf[0x140] = 0x03; //router
	buf[0x141] = 0x2b ; //vender imfo
	buf[0x142] = 0xff; //option end

	_enc->PacketSend(buf, 0x143);
}

int TinyIP::dhcp_offer(byte* buf)
{
	unsigned int i,i1,i2,i3,i4;

	i1=buf[DHCP_ID_H];
	i2=buf[DHCP_ID_H+1];
	i3=buf[DHCP_ID_H+2];
	i4=buf[DHCP_ID_H+3];
	i=(i1<<24)+(i2<<16)+(i3<<8)+i4;
	if(i == dhcp_id)   //判断是否发送给自己的dhcp offer
	{
		//装载ip
		fill_data(buf, MY_IP_H, IP, 0, 4);
		search_list_data(buf);
		return 1;
	}
	return 0;
}

void TinyIP::dhcp_request(byte* buf)
{
	dhcp_fill_public_data(buf);


	buf[0x10]=hex_to_dec_H(0x152-0xe); 	//长度为328 bytes
	buf[0x11]=hex_to_dec_L(0x152-0xe);

	fill_ip_hdr_checksum(buf);

	buf[0x26]=hex_to_dec_H(0x152-0xe-0x14);
	buf[0x27]=hex_to_dec_L(0x152-0xe-0x14);

	buf[0x28] = 0x00;
	buf[0x29] = 0x00;

	buf[0x11a] = 0x35; //option DHCP message type
	buf[0x11b] = 0x01; //lenth=1
	buf[0x11c] = 0x03; //dhcp request

	buf[0x11d] = 0x3d; //option client identifier
	buf[0x11e] = 0x07; //option 长度  1一个mac+一字节类型
	buf[0x11f] = 0x01; //类型ETHERNET=1

	buf[0x126] = 0x32; //requested ip option
	buf[0x127] = 0x04; //option lenth

	buf[0x128] = IP[0]; //请求的ip
	buf[0x129] = IP[1];
	buf[0x12a] = IP[2];
	buf[0x12b] = IP[3];


	buf[0x12c] = dhcp_option_server_id;
	buf[0x12d] = 0x04;
	buf[0x12e] = DHCPServer[0]; //dhcp server id
	buf[0x12f] = DHCPServer[1];
	buf[0x130] = DHCPServer[2];
	buf[0x131] = DHCPServer[3];


	buf[0x132] = 0x0c; //host option
	buf[0x133] = 0x04; //长度为4
	buf[0x134] = 0x61; //字符 a
	buf[0x135] = 0x62; //字符 b
	buf[0x136] = 0x63; //字符 c
	buf[0x137] = 0x64; //字符 d

	buf[0x138] = 0x51; 	//Client Fully Qualified Domain Name
	buf[0x139] = 0x07;
	buf[0x13a] = 0x00;
	buf[0x13b] = 0x00;
	buf[0x13c] = 0x00;
	buf[0x13d] = 0x61; //字符 a
	buf[0x13e] = 0x62; //字符 b
	buf[0x13f] = 0x63; //字符 c
	buf[0x140] = 0x64; //字符 d

	buf[0x141] = 0x3c; //vendor option
	buf[0x142] = 0x08; //长度为8
	buf[0x143] = 0x4d; //下面为vender信息
	buf[0x144] = 0x53;
	buf[0x145] = 0x46;
	buf[0x146] = 0x54;
	buf[0x147] = 0x20;
	buf[0x148] = 0x35;
	buf[0x149] = 0x2e;
	buf[0x14a] = 0x30;

	buf[0x14b] = 0x37; //请求列表
	buf[0x14c] = 0x04; //长度
	buf[0x14d] = 0x01; //mask
	buf[0x14e] = 0x06; //dns
	buf[0x14f] = 0x03; //router
	buf[0x150] = 0x2b ; //vender imfo
	buf[0x151] = 0xff; //option end

	_enc->PacketSend(buf, 0x152);
}

int TinyIP::dhcp_ack(byte* buf)
{
	if(buf[MY_IP_H] == IP[0] && buf[MY_IP_H+1] == IP[1] &&
	   buf[MY_IP_H+2] == IP[2] && buf[MY_IP_H+3] == IP[3])
	{
		IPIsReady = 1;
		return 1;
	}

	return 0;
}

void TinyIP::fill_data(byte *src, int src_begin, byte *dst, int dst_begin, int len)
{
	for(int i=0; i<len; i++, dst_begin++, src_begin++)
		dst[dst_begin] = src[src_begin];
}

void TinyIP::search_list_data(byte* buf)
{
	char find=0,len=0;
	int begin = dhcp_option_type_h;
	while(1)
	{
		if(buf[begin] == dhcp_option_mask)
		{
			fill_data(buf, begin + 2, Mask, 0, 4);
			len = buf[begin + 1] + 2;
			begin += len;
			find++;
		}
		else if(buf[begin] == dhcp_option_dns)
		{
			fill_data(buf, begin + 2, DNSServer, 0, 4);
			len = buf[begin + 1] + 2;
			begin += len;
			find++;
		}
		else if(buf[begin] == dhcp_option_router)
		{
			fill_data(buf, begin + 2, Gateway, 0, 4);
			len = buf[begin + 1] + 2;
			begin += len;
			find++;
		}
		else if(buf[begin] == dhcp_option_server_id)
		{
			fill_data(buf, begin + 2, DHCPServer, 0, 4);
			len = buf[begin + 1] + 2;
			begin += len;
			find++;
		}
		else
		{
			len = buf[begin + 1];
			begin += len + 2;
		}

		if(find == 4) break;
	}
}

void TinyIP::dhcp_fill_public_data(byte* buf)
{
	char i=0;
	for(i=0;i<6;i++)                    				//填充以太网头部mac
	{
		buf[ETH_DST_MAC + i] = 0xff;
		buf[ETH_SRC_MAC + i] = Mac[i];
		buf[0x46 + i] = Mac[i];											//client mac
		buf[0x120 + i] = Mac[i];										//option client mac
	}

	buf[0x0c] = 0x08;     												//0x80 0x00  ip包
	buf[0x0d] = 0x00;

	buf[0x0e] = 0x45;     												//4代表 ipv4 5代表5*4bytes=20 bytes ipv4头部长度
	buf[0x0f] = 0x00;

	buf[0x14] = 0x00;                             //fragment offset =0
	buf[0x15] = 0x00;

	buf[0x16] = 0x40; 	//ttl=64
	buf[0x17] = 0x11; 	//udp协议

	for(i=0;i<4;i++)													  //填充ip
	{
		buf[IP_SRC_P+i] = 0x00;
		buf[IP_DST_P+i] = 0xff;
	}

	buf[0x22] = 0x00;
	buf[0x23] = 0x44;                             //本地dhcp端口为68

	buf[0x24] = 0x00;
	buf[0x25] = 0x43; 	//dhcp服务器端口

	buf[0x2a] = 0x01;                             //boot requese
	buf[0x2b] = 0x01; 	//硬件类型  ethernet

	buf[0x2c] = 0x06; 	//硬件地址长度 6

	buf[0x2e]=dhcp_id>>24;								//dhcp识别码
	buf[0x2f]=dhcp_id>>16&0xff;
	buf[0x30]=dhcp_id>>8&0xff;
	buf[0x31]=dhcp_id&0xff;

	buf[0x34] = 0x80; 	//最左一bit为1时表示server将以广播方式传递封包给client，其余尚未使用

	buf[0x116] = 0x63; //DHCP
	buf[0x117] = 0x82;
	buf[0x118] = 0x53;
	buf[0x119] = 0x63;
}

void TinyIP::DHCP_config(byte* buf)
{
	dhcp_discover(buf);
	while(1)
	{
		uint len = _enc->PacketReceive(buf, BufferSize);
		if(!len) continue;

		if(buf[dhcp_protocol_h]==0x63 && buf[0x11c]==0x02 && buf[0x25]==0x44)
		{
			if(dhcp_offer(buf))
			{
				dhcp_request(buf);
				continue;
			}
		}
		if(buf[dhcp_protocol_h]==0x63 && buf[0x11c]==0x05 && buf[0x25]==0x44)
		{
			if(dhcp_ack(buf)) break;
			continue;
		}
	}
}
