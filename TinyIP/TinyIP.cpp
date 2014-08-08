#include "TinyIP.h"

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
    // fill the header:
    // This code requires that we send only one data packet
    // because we keep no state information. We must therefore set
    // the fin here:
    buf[TCP_FLAGS_P] = TCP_FLAGS_ACK_V|TCP_FLAGS_PUSH_V|TCP_FLAGS_FIN_V;

    // total length field in the IP header must be set:
    // 20 bytes IP + 20 bytes tcp (when no options) + len of data
    uint j = IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + size;
    buf[IP_TOTLEN_H_P] = j >> 8;
    buf[IP_TOTLEN_L_P] = j & 0xff;
    fill_ip_hdr_checksum(buf);
    // zero the checksum
    buf[TCP_CHECKSUM_H_P] = 0;
    buf[TCP_CHECKSUM_L_P] = 0;
    // calculate the checksum, len=8 (start from ip.src)  +  TCP_HEADER_LEN_PLAIN  +  data len
    j = checksum(&buf[IP_SRC_P], 8 + TCP_HEADER_LEN_PLAIN + size,2);
    buf[TCP_CHECKSUM_H_P] = j >> 8;
    buf[TCP_CHECKSUM_L_P] = j & 0xff;
    _enc->PacketSend(buf, IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + size + ETH_HEADER_LEN);
}

void TinyIP::TcpSend(byte* buf, uint size)
{
    // fill the header:
    // This code requires that we send only one data packet
    // because we keep no state information. We must therefore set
    // the fin here:
    buf[TCP_FLAGS_P] = TCP_FLAGS_ACK_V|TCP_FLAGS_PUSH_V;

    // total length field in the IP header must be set:
    // 20 bytes IP  +  20 bytes tcp (when no options)  +  len of data
    uint j = IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + size;
    buf[IP_TOTLEN_H_P] = j >> 8;
    buf[IP_TOTLEN_L_P] = j & 0xff;
    fill_ip_hdr_checksum(buf);
    // zero the checksum
    buf[TCP_CHECKSUM_H_P] = 0;
    buf[TCP_CHECKSUM_L_P] = 0;
    // calculate the checksum, len=8 (start from ip.src)  +  TCP_HEADER_LEN_PLAIN  +  data len
    j = checksum(&buf[IP_SRC_P], 8 + TCP_HEADER_LEN_PLAIN + size, 2);
    buf[TCP_CHECKSUM_H_P] = j >> 8;
    buf[TCP_CHECKSUM_L_P] = j & 0xff;
		//debug_printf("len=%d\r\n",tcp_d_len);
    _enc->PacketSend(buf, IP_HEADER_LEN + TCP_HEADER_LEN_PLAIN + size + ETH_HEADER_LEN);
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

    while(1)
    {
        // 获取缓冲器的包
        uint len = _enc->PacketReceive(buf, BufferSize);

        // 如果缓冲器里面没有数据则转入下一次循环
        if(!len) continue;

        if(eth_type_is_arp_and_my_ip(buf, len))
        {
            make_arp_answer_from_request(buf);
            continue;
        }

        debug_printf("Packet: %d\r\n", len);

        for(int i=0; i<4; i++)
        {
            debug_printf("begin:\r\n");
            debug_printf("TCP_FLAGS_P %d\r\n", buf[TCP_FLAGS_P]);
            debug_printf("\r\n");
        }

        // 判断是否为发送给我们ip的包
        if(!eth_type_is_ip_and_my_ip(buf, len)) continue;

        // ICMP协议检测与检测是否为ICMP请求 ping
        if(buf[IP_PROTO_P] == IP_PROTO_ICMP_V)
        {
			ProcessICMP(buf, len);
            continue;
        }

        //用于查看数据包的标志位
        if (buf[IP_PROTO_P] == IP_PROTO_TCP_V)
        {
			ProcessTcp(buf, len);
			continue;
        }
        if (buf[IP_PROTO_P] == IP_PROTO_UDP_V /*&& buf[UDP_DST_PORT_H_P] == 4*/)
        {
			ProcessUdp(buf, len);
			continue;
        }

        debug_printf("Something accessed me....\r\n");
    }
}

void TinyIP::ProcessICMP(byte* buf, uint len)
{
	if(buf[ICMP_TYPE_P] != ICMP_TYPE_ECHOREQUEST_V) return;

	debug_printf("from "); // 打印发方的ip
	ShowIP(&buf[IP_SRC_P]);
	debug_printf(" ICMP package.\r\n");
	make_echo_reply_from_request(buf, len);
}

void TinyIP::ProcessTcp(byte* buf, uint len)
{
	//获取目标机的TCP端口
	//ushort tcp_port = buf[TCP_DST_PORT_H_P] << 8 | buf[TCP_DST_PORT_L_P];

	// 第一次同步应答
	if (buf[TCP_FLAGS_P] & TCP_FLAGS_SYN_V) // SYN连接请求标志位，为1表示发起连接的请求数据包
	{
		debug_printf("One TCP request from "); // 打印发送方的ip
		ShowIP(&buf[IP_SRC_P]);
		debug_printf("\r\n");

		//第二次同步应答
		make_tcp_synack_from_syn(buf);

		return;
	}
	// 第三次同步应答,三次应答后方可传输数据
	if (buf[TCP_FLAGS_P] & TCP_FLAGS_ACK_V) // ACK确认标志位，为1表示此数据包为应答数据包
	{
		init_len_info(buf);
		uint dat_p = get_tcp_data_pointer();

		//无数据返回ACK
		if (dat_p == 0)
		{
			if (buf[TCP_FLAGS_P] & TCP_FLAGS_FIN_V)      //FIN结束连接请求标志位。为1表示是结束连接的请求数据包
			{
				make_tcp_ack_from_any(buf);
			}
			return;
		}
		///////////////////////////打印TCP数据/////////////////
		debug_printf("Data from TCP:");
		uint i = 0;
		while(i < tcp_d_len)
		{
			debug_printf("%c", buf[dat_p + i]);
			i++;
		}
		debug_printf("\r\n");
		///////////////////////////////////////////////////////
		make_tcp_ack_from_any(buf);       // 发送ACK，通知已收到
		TcpSend(buf, len);

		// tcp_close(buf,len);
		// for(;reset<BufferSize + 1;reset++)
		// 	buf[BufferSize + 1] = 0;
	}
}

void TinyIP::ProcessUdp(byte* buf, uint len)
{
	//获取目标机UDP的端口
	//ushort udp_port = buf[UDP_DST_PORT_H_P] << 8 | buf[UDP_DST_PORT_L_P];

	debug_printf("Data from UDP:");
	//UDP数据长度
	uint payloadlen = buf[UDP_LEN_H_P];
	payloadlen = payloadlen << 8;
	payloadlen = (payloadlen + buf[UDP_LEN_L_P]) - UDP_HEADER_LEN;

	byte* buf2 = new byte[payloadlen];
	for(int i=0; i<payloadlen; i++)
	{
		buf2[i] = buf[UDP_DATA_P + i];
		debug_printf("%c", buf2[i]);
	}
	debug_printf("\r\n");

	//获取发送源端口
	ushort pc_port = buf[UDP_SRC_PORT_H_P] << 8 | buf[UDP_SRC_PORT_L_P];
	make_udp_reply_from_request(buf, buf2, payloadlen, pc_port);
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

// The Ip checksum is calculated over the ip header only starting
// with the header length field and a total length of 20 bytes
// unitl ip.dst
// You must set the IP checksum field to zero before you start
// the calculation.
// len for ip is 20.
//
// For UDP/TCP we do not make up the required pseudo header. Instead we
// use the ip.src and ip.dst fields of the real packet:
// The udp checksum calculation starts with the ip.src field
// Ip.src=4bytes,Ip.dst=4 bytes,Udp header=8bytes + data length=16+len
// In other words the len here is 8 + length over which you actually
// want to calculate the checksum.
// You must set the checksum field to zero before you start
// the calculation.
// len for udp is: 8 + 8 + data length
// len for tcp is: 4+4 + 20 + option len + data length
//
// For more information on how this algorithm works see:
// http://www.netfor2.com/checksum.html
// http://www.msc.uky.edu/ken/cs471/notes/chap3.htm
// The RFC has also a C code example: http://www.faqs.org/rfcs/rfc1071.html
uint TinyIP::checksum(byte* buf, uint len, byte type)
{
    // type 0=ip
    //      1=udp
    //      2=tcp
    unsigned long sum = 0;

    //if(type==0){
    //        // do not add anything
    //}
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
    while(len >1)
    {
        sum += 0xFFFF & (*buf<<8|*(buf+1));
        buf+=2;
        len-=2;
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

//检查是否为合法的eth，并且只接收发给本机的arp数据
byte TinyIP::eth_type_is_arp_and_my_ip(byte* buf, uint len)
{
    //arp报文长度判断，正常的arp报文长度为42byts
    if (len < 41) return false;

	//检测是否为arp报文
    if(buf[ETH_TYPE_H_P] != ETHTYPE_ARP_H_V || buf[ETH_TYPE_L_P] != ETHTYPE_ARP_L_V) return false;

    for(int i=0; i<4; i++)
    {
        if(buf[ETH_ARP_DST_IP_P + i] != IP[i]) return false;
    }
    return true;
}

byte TinyIP::eth_type_is_ip_and_my_ip(byte* buf, uint len)
{
    byte i=0;
    //eth+ip+udp header is 42
    if (len < 42) return false;

    if(buf[ETH_TYPE_H_P]!=ETHTYPE_IP_H_V || buf[ETH_TYPE_L_P]!=ETHTYPE_IP_L_V) return false;

    if (buf[IP_HEADER_LEN_VER_P] != 0x45)
    {
        // must be IP V4 and 20 byte header
        return false;
    }
    while(i<4)
    {
        if(buf[IP_DST_P + i] != IP[i])
        {
            return false;
        }
        i++;
    }
    return true;
}
// make a return eth header from a received eth packet
void TinyIP::make_eth(byte* buf)
{
    //copy the destination mac from the source and fill my mac into src
    for(int i=0; i<6; i++)
    {
        buf[ETH_DST_MAC +i] = buf[ETH_SRC_MAC +i];
        buf[ETH_SRC_MAC +i] = Mac[i];
    }
}
void TinyIP::fill_ip_hdr_checksum(byte* buf)
{
    uint ck;
    // clear the 2 byte checksum
    buf[IP_CHECKSUM_P] = 0;
    buf[IP_CHECKSUM_P + 1] = 0;
    buf[IP_FLAGS_P] = 0x40; // don't fragment
    buf[IP_FLAGS_P + 1] = 0;  // fragement offset
    buf[IP_TTL_P] = 64; // ttl
    // calculate the checksum:
    ck = checksum(&buf[IP_P], IP_HEADER_LEN, 0);
    buf[IP_CHECKSUM_P] = ck >> 8;
    buf[IP_CHECKSUM_P + 1] = ck & 0xff;
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
    byte i=0;
    byte tseq;
    while(i<2)
    {
        buf[TCP_DST_PORT_H_P + i] = buf[TCP_SRC_PORT_H_P + i];
        buf[TCP_SRC_PORT_H_P + i] = 0; // clear source port
        i++;
    }
    // set source port  (http):
   // buf[TCP_SRC_PORT_L_P] = wwwport;                //源码//////////////////////////////////////////////
		//debug_printf("%d\r\n",src_port_H<<8 | src_port_L);
	buf[TCP_SRC_PORT_H_P] = RemotePort >> 8;								//自己添加
	buf[TCP_SRC_PORT_L_P] = RemotePort & 0xFF;								//

    i=4;
    // sequence numbers:
    // add the rel ack num to SEQACK
    while(i>0)
    {
        rel_ack_num = buf[TCP_SEQ_H_P + i-1] + rel_ack_num;
        tseq = buf[TCP_SEQACK_H_P + i-1];
        buf[TCP_SEQACK_H_P + i-1] = 0xff&rel_ack_num;
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
        buf[TCP_SEQ_H_P + 0] = 0;
        buf[TCP_SEQ_H_P + 1] = 0;
        // we step only the second byte, this allows us to send packts
        // with 255 bytes or 512 (if we step the initial seqnum by 2)
        buf[TCP_SEQ_H_P + 2] = seqnum;
        buf[TCP_SEQ_H_P + 3] = 0;
        // step the inititial seq num by something we will not use
        // during this tcp session:
        seqnum += 2;
    }
    // zero the checksum
    buf[TCP_CHECKSUM_H_P] = 0;
    buf[TCP_CHECKSUM_L_P] = 0;

    // The tcp header length is only a 4 bit field (the upper 4 bits).
    // It is calculated in units of 4 bytes.
    // E.g 24 bytes: 24/4=6 => 0x60=header len field
    //buf[TCP_HEADER_LEN_P]=(((TCP_HEADER_LEN_PLAIN+4)/4)) <<4; // 0x60
    if (mss)
    {
        // the only option we set is MSS to 1408:
        // 1408 in hex is 0x580
        buf[TCP_OPTIONS_P] = 2;
        buf[TCP_OPTIONS_P + 1] = 4;
        buf[TCP_OPTIONS_P + 2] = 0x05;
        buf[TCP_OPTIONS_P + 3] = 0x80;
        // 24 bytes:
        buf[TCP_HEADER_LEN_P] = 0x60;
    }
    else
    {
        // no options:
        // 20 bytes:
        buf[TCP_HEADER_LEN_P] = 0x50;
    }
}

void TinyIP::make_arp_answer_from_request(byte* buf)
{
    make_eth(buf);
    buf[ETH_ARP_OPCODE_H_P] = ETH_ARP_OPCODE_REPLY_H_V;   //arp 响应
    buf[ETH_ARP_OPCODE_L_P] = ETH_ARP_OPCODE_REPLY_L_V;
    // fill the mac addresses:
    for(int i=0; i<6; i++)
    {
        buf[ETH_ARP_DST_MAC_P + i] = buf[ETH_ARP_SRC_MAC_P + i];
        buf[ETH_ARP_SRC_MAC_P + i] = Mac[i];
    }
    for(int i=0; i<4; i++)
    {
        buf[ETH_ARP_DST_IP_P + i] = buf[ETH_ARP_SRC_IP_P + i];
        buf[ETH_ARP_SRC_IP_P + i] = IP[i];
    }
    // eth+arp is 42 bytes:
    _enc->PacketSend(buf, 42);
}

void TinyIP::make_echo_reply_from_request(byte* buf, uint len)
{
    make_eth(buf);
    make_ip(buf);
    buf[ICMP_TYPE_P] = ICMP_TYPE_ECHOREPLY_V;	  //////回送应答////////////////////////////////////////////////////////////////////////////
    // we changed only the icmp.type field from request(=8) to reply(=0).
    // we can therefore easily correct the checksum:
    if (buf[ICMP_CHECKSUM_P] > (0xff-0x08))
    {
        buf[ICMP_CHECKSUM_P + 1]++;
    }
    buf[ICMP_CHECKSUM_P]+=0x08;
    //
    _enc->PacketSend(buf, len);
}

// you can send a max of 220 bytes of data
void TinyIP::make_udp_reply_from_request(byte* buf,byte *data, uint datalen, uint port)
{
    make_eth(buf);
    //if (datalen>220)
    //	{
    //    datalen=220;
    //	}

    // total length field in the IP header must be set:
    uint i = IP_HEADER_LEN+UDP_HEADER_LEN+datalen;
    buf[IP_TOTLEN_H_P] = i >> 8;
    buf[IP_TOTLEN_L_P] = i;
    make_ip(buf);
    buf[UDP_DST_PORT_H_P] = port >> 8;
    buf[UDP_DST_PORT_L_P] = port & 0xff;
    // source port does not matter and is what the sender used.
    // calculte the udp length:
    buf[UDP_LEN_H_P] = datalen >> 8;
    buf[UDP_LEN_L_P] = UDP_HEADER_LEN+datalen;
    // zero the checksum
    buf[UDP_CHECKSUM_H_P] = 0;
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
    _enc->PacketSend(buf, UDP_HEADER_LEN+IP_HEADER_LEN+ETH_HEADER_LEN+datalen);
}

void TinyIP::make_tcp_synack_from_syn(byte* buf)
{
    uint ck;
    make_eth(buf);
    // total length field in the IP header must be set:
    // 20 bytes IP + 24 bytes (20tcp+4tcp options)
    buf[IP_TOTLEN_H_P] = 0;
    buf[IP_TOTLEN_L_P] = IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+4;
    make_ip(buf);
    buf[TCP_FLAGS_P] = TCP_FLAGS_SYNACK_V;
    make_tcphead(buf,1,1,0);
    // calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + 4 (one option: mss)
    ck=checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN+4,2);
    buf[TCP_CHECKSUM_H_P] = ck >> 8;
    buf[TCP_CHECKSUM_L_P] = ck & 0xff;
    // add 4 for option mss:
    _enc->PacketSend(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+4+ETH_HEADER_LEN);
}

// get a pointer to the start of tcp data in buf
// Returns 0 if there is no data
// You must call init_len_info once before calling this function
uint TinyIP::get_tcp_data_pointer(void)
{
    if (info_data_len)
    {
        return((uint)TCP_SRC_PORT_H_P + info_hdr_len);
    }
    else
    {
        return false;
    }
}

// do some basic length calculations and store the result in static varibales
void TinyIP::init_len_info(byte* buf)
{
	  //IP包长度
    info_data_len=(buf[IP_TOTLEN_H_P]<<8)|(buf[IP_TOTLEN_L_P]&0xff);
	  buf_len=info_data_len;
	  //减去IP首部长度
    info_data_len-=IP_HEADER_LEN;
	  //TCP首部长度，因为TCP协议规定了只有四位来表示长度，所以需要以下处理,4*6=24
    info_hdr_len=(buf[TCP_HEADER_LEN_P]>>4)*4; // generate len in bytes;
	  //减去TCP首部长度
    info_data_len-=info_hdr_len;
	tcp_d_len=info_data_len;
    if (info_data_len<=0)
    {
        info_data_len=0;
    }

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
    while ((c = *progmem_s++))
    {
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
void TinyIP::make_tcp_ack_from_any(byte* buf)
{
    uint j;
    make_eth(buf);
    // fill the header:
    buf[TCP_FLAGS_P] = TCP_FLAGS_ACK_V;

    if (info_data_len == 0)
    {
        // if there is no data then we must still acknoledge one packet
        make_tcphead(buf,1,0,1); // no options
    }
    else
    {
        make_tcphead(buf,info_data_len,0,1); // no options
    }

    // total length field in the IP header must be set:
    // 20 bytes IP + 20 bytes tcp (when no options)
    j=IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN;
    buf[IP_TOTLEN_H_P] = j >> 8;
    buf[IP_TOTLEN_L_P] = j& 0xff;
    make_ip(buf);
    // calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + data len
    j=checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN,2);
    buf[TCP_CHECKSUM_H_P] = j >> 8;
    buf[TCP_CHECKSUM_L_P] = j& 0xff;
    _enc->PacketSend(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+ETH_HEADER_LEN);
}

// you must have called init_len_info at some time before calling this function
// dlen is the amount of tcp data (http data) we send in this packet
// You can use this function only immediately after make_tcp_ack_from_any
// This is because this function will NOT modify the eth/ip/tcp header except for
// length and checksum
void TinyIP::make_tcp_ack_with_data(byte* buf, uint dlen)
{
    uint j;
    // fill the header:
    // This code requires that we send only one data packet
    // because we keep no state information. We must therefore set
    // the fin here:
    buf[TCP_FLAGS_P] = TCP_FLAGS_ACK_V|TCP_FLAGS_PUSH_V;//|TCP_FLAGS_FIN_V;

    // total length field in the IP header must be set:
    // 20 bytes IP + 20 bytes tcp (when no options) + len of data
    j=IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+dlen;
    buf[IP_TOTLEN_H_P] = j >> 8;
    buf[IP_TOTLEN_L_P] = j& 0xff;
    fill_ip_hdr_checksum(buf);
    // zero the checksum
    buf[TCP_CHECKSUM_H_P] = 0;
    buf[TCP_CHECKSUM_L_P] = 0;
    // calculate the checksum, len=8 (start from ip.src) + TCP_HEADER_LEN_PLAIN + data len
    j=checksum(&buf[IP_SRC_P], 8+TCP_HEADER_LEN_PLAIN+dlen,2);
    buf[TCP_CHECKSUM_H_P] = j >> 8;
    buf[TCP_CHECKSUM_L_P] = j& 0xff;
    _enc->PacketSend(buf, IP_HEADER_LEN+TCP_HEADER_LEN_PLAIN+dlen+ETH_HEADER_LEN);
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
