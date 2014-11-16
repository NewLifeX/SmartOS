#include "Tcp.h"

bool Callback(TinyIP* tip, void* param);

TcpSocket::TcpSocket(TinyIP* tip) : Socket(tip)
{
	Type = IP_TCP;

	Port		= 0;
	RemoteIP	= 0;
	RemotePort	= 0;
	LocalIP		= 0;
	LocalPort	= 0;

	seqnum		= 0xa;

	Status = Closed;
}

bool TcpSocket::Process(MemoryStream* ms)
{
	TCP_HEADER* tcp = (TCP_HEADER*)ms->Current();
	if(!ms->Seek(tcp->Length << 2)) return false;

	Header = tcp;
	uint len = ms->Remain();

	ushort port = __REV16(tcp->DestPort);
	ushort remotePort = __REV16(tcp->SrcPort);

	// 仅处理本连接的IP和端口
	if(Port != 0 && port != Port) return false;
	if(RemotePort != 0 && remotePort != RemotePort) return false;
	if(RemoteIP != 0 && Tip->RemoteIP != RemoteIP) return false;

	// 不能修改主监听Socket的端口，否则可能导致收不到后续连接数据
	//Port = port;
	//RemotePort = remotePort;
	// 
	//Tip->Port = port;
	//Tip->RemotePort = remotePort;

	// 第一次同步应答
	if (tcp->Flags & TCP_FLAGS_SYN && !(tcp->Flags & TCP_FLAGS_ACK)) // SYN连接请求标志位，为1表示发起连接的请求数据包
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
		Head(tcp, 1, true, false);

		// 需要用到MSS，所以采用4个字节的可选段
		//Send(tcp, 4, TCP_FLAGS_SYN | TCP_FLAGS_ACK);
		// 注意tcp->Size()包括头部的扩展数据，这里不用单独填4
		Send(tcp, 0, TCP_FLAGS_SYN | TCP_FLAGS_ACK);

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
				Head(tcp, 1, false, true);
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
				Head(tcp, 1, false, true);
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
			Sys.ShowString(tcp->Next(), len);
			debug_printf("\r\n");
#endif
		}
		// 发送ACK，通知已收到
		Head(tcp, len, false, true);
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
			Head(tcp, 1, false, true);
			//Close(tcp, 0);
			Send(tcp, 0, TCP_FLAGS_ACK | TCP_FLAGS_PUSH | TCP_FLAGS_FIN);
		}
	}

	return true;
}

void TcpSocket::Send(TCP_HEADER* tcp, uint len, byte flags)
{
	Tip->RemoteIP = RemoteIP;

	tcp->SrcPort = __REV16(Port);
	tcp->DestPort = __REV16(RemotePort);
    tcp->Flags = flags;
	if(tcp->Length < sizeof(TCP_HEADER) / 4) tcp->Length = sizeof(TCP_HEADER) / 4;

	// 网络序是大端
	tcp->Checksum = 0;
	tcp->Checksum = __REV16((ushort)TinyIP::CheckSum((byte*)tcp - 8, 8 + sizeof(TCP_HEADER) + len, 2));

	debug_printf("SendTcp: Flags=0x%02x, len=%d(0x%x) %d => %d \r\n", flags, tcp->Length, tcp->Length, __REV16(tcp->SrcPort), __REV16(tcp->DestPort));

	// 注意tcp->Size()包括头部的扩展数据
	Tip->SendIP(IP_TCP, (byte*)tcp, tcp->Size() + len);
}

void TcpSocket::Head(TCP_HEADER* tcp, uint ackNum, bool mss, bool opSeq)
{
    /*
	第一次握手：主机A发送位码为SYN＝1，随机产生Seq=1234567的数据包到服务器，主机B由SYN=1知道，A要求建立联机
	第二次握手：主机B收到请求后要确认联机信息，向A发送Ack=(主机A的Seq+1)，SYN=1，Ack=1，随机产生Seq=7654321的包
	第三次握手：主机A收到后检查Ack是否正确，即第一次发送的Seq+1，以及位码Ack是否为1，
	若正确，主机A会再发送Ack=(主机B的Seq+1)，Ack=1，主机B收到后确认Seq值与Ack=1则连接建立成功。
	完成三次握手，主机A与主机B开始传送数据。
	*/
	//TCP_HEADER* tcp = Header;
	int ack = tcp->Ack;
	tcp->Ack = __REV(__REV(tcp->Seq) + ackNum);
    if (!opSeq)
    {
		// 我们仅仅递增第二个字节，这将允许我们以256或者512字节来发包
		tcp->Seq = __REV(seqnum << 8);
        // step the inititial seq num by something we will not use
        // during this tcp session:
        seqnum += 2;
		/*tcp->Seq = __REV(seqnum);
		seqnum++;*/
		//tcp->Seq = 0;
    }else
	{
		tcp->Seq = ack;
	}

	tcp->Length = sizeof(TCP_HEADER) / 4;
    // 头部后面可能有可选数据，Length决定头部总长度（4的倍数）
    if (mss)
    {
		uint* p = (uint*)tcp->Next();
        // 使用可选域设置 MSS 到 1460:0x5b4
		p[0] = __REV(0x020405b4);
		p[1] = __REV(0x01030302);
		p[2] = __REV(0x01010402);

		tcp->Length += 3;
    }
}

TCP_HEADER* TcpSocket::Create()
{
	return (TCP_HEADER*)(Tip->Buffer + sizeof(ETH_HEADER) + sizeof(IP_HEADER));
}

void TcpSocket::Ack(uint len)
{
	TCP_HEADER* tcp = Create();
	tcp->Init(true);
	Send(tcp, len, TCP_FLAGS_ACK | TCP_FLAGS_PUSH);
}

void TcpSocket::Close()
{
	debug_printf("Tcp::Close ");
	Tip->ShowIP(RemoteIP);
	debug_printf(":%d \r\n", RemotePort);

	TCP_HEADER* tcp = Create();
	tcp->Init(true);
	Send(tcp, 0, TCP_FLAGS_ACK | TCP_FLAGS_PUSH | TCP_FLAGS_FIN);
}

void TcpSocket::Send(byte* buf, uint len)
{
	debug_printf("Tcp::Send ");
	Tip->ShowIP(RemoteIP);
	debug_printf(":%d buf=0x%08x len=%d ...... \r\n", RemotePort, buf, len);

	TCP_HEADER* tcp = Create();
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

	Tip->LoopWait(Callback, this, 5000);

	if(tcp->Flags & TCP_FLAGS_ACK)
		debug_printf("发送成功！\r\n");
	else
		debug_printf("发送失败！\r\n");
}

// 连接远程服务器，记录远程服务器IP和端口，后续发送数据和关闭连接需要
bool TcpSocket::Connect(IPAddress ip, ushort port)
{
	debug_printf("Tcp::Connect ");
	Tip->ShowIP(ip);
	debug_printf(":%d ...... \r\n", port);

	RemoteIP = ip;
	RemotePort = port;

	TCP_HEADER* tcp = Create();
	tcp->Init(true);
	tcp->Seq = 0;
	//seqnum = 0;
	Head(tcp, 0, true, false);

	Status = SynSent;
	Send(tcp, 0, TCP_FLAGS_SYN);

	if(Tip->LoopWait(Callback, this, 5000))
	{
		if(tcp->Flags & TCP_FLAGS_SYN)
		{
			Status = Established;
			debug_printf("连接成功！\r\n");
			return true;
		}
		Status = Closed;
		debug_printf("拒绝连接！\r\n");
		return false;
	}

	Status = Closed;
	debug_printf("连接超时！\r\n");
	return false;
}

bool Callback(TinyIP* tip, void* param)
{
	ETH_HEADER* eth = (ETH_HEADER*)tip->Buffer;
	if(eth->Type != ETH_IP) return false;

	IP_HEADER* _ip = (IP_HEADER*)eth->Next();
	if(_ip->Protocol != IP_TCP) return false;

	TcpSocket* socket = (TcpSocket*)param;
	TCP_HEADER* tcp = (TCP_HEADER*)_ip->Next();
	// 检查端口
	if(tcp->DestPort != socket->Port) return false;

	socket->Header = tcp;

	//if(Status == SynSent)
	{
		// 处理。如果对方回发第二次握手包，或者终止握手
		MemoryStream ms(tip->Buffer, tip->BufferSize);
		socket->Process(&ms);
	}
	//else
	{
	}

	return true;
}
