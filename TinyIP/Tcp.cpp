#include "Tcp.h"

#define NET_DEBUG DEBUG


bool Callback(TinyIP* tip, void* param, MemoryStream& ms);

TcpSocket::TcpSocket(TinyIP* tip) : Socket(tip)
{
	Type = IP_TCP;

	Port		= 0;
	RemoteIP	= 0;
	RemotePort	= 0;
	LocalIP		= 0;
	LocalPort	= 0;

	// 累加端口
	static ushort g_tcp_port = 1024;
	if(g_tcp_port < 1024) g_tcp_port = 1024;
	BindPort = g_tcp_port++;

	// 我们仅仅递增第二个字节，这将允许我们以256或者512字节来发包
	static uint seqnum = 0xa;
	Seq = seqnum << 8;
	seqnum += 2;

	Ack = 0;

	Status = Closed;
	
	OnAccepted = NULL;
	OnReceived = NULL;
	OnDisconnected = NULL;
}

string TcpSocket::ToString()
{
	static char name[10];
	sprintf(name, "TCP_%d", Port);
	return name;
}

bool TcpSocket::OnOpen()
{
	if(Port != 0) BindPort = Port;

	if(Port)
		debug_printf("Tcp::Open %d 过滤 %d\r\n", BindPort, Port);
	else
		debug_printf("Tcp::Open %d 监听所有端口TCP数据包\r\n", BindPort);

	Enable = true;
	return Enable;
}

void TcpSocket::OnClose()
{
	debug_printf("Tcp::Close %d\r\n", BindPort);
	Enable = false;
}

bool TcpSocket::Process(MemoryStream* ms)
{
	TCP_HEADER* tcp = (TCP_HEADER*)ms->Current();
	if(!ms->Seek(tcp->Size())) return false;

	Header = tcp;
	uint len = ms->Remain();

	ushort port = __REV16(tcp->DestPort);
	ushort remotePort = __REV16(tcp->SrcPort);

	// 仅处理本连接的IP和端口
	if(Port != 0 && port != Port) return false;
	//if(RemotePort != 0 && remotePort != RemotePort) return false;
	//if(RemoteIP != 0 && Tip->RemoteIP != RemoteIP) return false;

	IP_HEADER* ip = tcp->Prev();
	RemotePort	= remotePort;
	RemoteIP	= ip->SrcIP;
	LocalPort	= port;
	LocalIP		= ip->DestIP;

	OnProcess(tcp, *ms);

	return true;
}

void TcpSocket::OnProcess(TCP_HEADER* tcp, MemoryStream& ms)
{
	// 计算标称的数据长度
	//uint len = tcp->Size() - sizeof(TCP_HEADER);
	//assert_param(len <= ms.Remain());
	// TCP好像没有标识数据长度的字段，但是IP里面有，这样子的话，ms里面的长度是准确的
	uint len = ms.Remain();

#if NET_DEBUG
	debug_printf("Tcp::Process Flags=0x%02x From ", tcp->Flags);
	TinyIP::ShowIP(RemoteIP);
	debug_printf(":%d", RemotePort);
	debug_printf("\r\n");
#endif

	// 下次主动发数据时，用该序列号，因为对方Ack确认期望下次得到这个序列号
	Seq = __REV(tcp->Ack);
	Ack = __REV(tcp->Seq) + len + 1;

	// 第一次同步应答
	if (tcp->Flags & TCP_FLAGS_SYN) // SYN连接请求标志位，为1表示发起连接的请求数据包
	{
		if(!(tcp->Flags & TCP_FLAGS_ACK))
			OnAccept(tcp, len);
		else
			Accepted2(tcp, len);
	}
	else if(tcp->Flags & (TCP_FLAGS_FIN | TCP_FLAGS_RST))
	{
		OnDisconnect(tcp, len);
	}
	// 第三次同步应答,三次应答后方可传输数据
	else if (tcp->Flags & TCP_FLAGS_ACK) // ACK确认标志位，为1表示此数据包为应答数据包
	{
		if(len == 0 && tcp->Ack <= 1)
			Accepted2(tcp, len);
		else
			OnDataReceive(tcp, len);
	}
	else
		debug_printf("Tcp::Process 未知标识位 0x%02x \r\n", tcp->Flags);
}

void TcpSocket::OnAccept(TCP_HEADER* tcp, uint len)
{
	if(OnAccepted)
		OnAccepted(this, tcp, tcp->Next(), len);
	else
	{
#if NET_DEBUG
		debug_printf("Tcp Accept "); // 打印发送方的ip
		TinyIP::ShowIP(RemoteIP);
		debug_printf(":%d", RemotePort);
		debug_printf("\r\n");
#endif
	}

	//第二次同步应答
	SetSeqAck(tcp, 1, false);
	SetMss(tcp);

	// 需要用到MSS，所以采用4个字节的可选段
	//Send(tcp, 4, TCP_FLAGS_SYN | TCP_FLAGS_ACK);
	// 注意tcp->Size()包括头部的扩展数据，这里不用单独填4
	Send(tcp, 0, TCP_FLAGS_SYN | TCP_FLAGS_ACK);
}

void TcpSocket::Accepted2(TCP_HEADER* tcp, uint len)
{
	if(OnAccepted)
		OnAccepted(this, tcp, tcp->Next(), len);
	else
	{
#if NET_DEBUG
		debug_printf("Tcp Accept3 "); // 打印发送方的ip
		TinyIP::ShowIP(RemoteIP);
		debug_printf(":%d", RemotePort);
		debug_printf("\r\n");
#endif
	}

	//第二次同步应答
	SetSeqAck(tcp, 1, true);

	Send(tcp, 0, TCP_FLAGS_ACK);
}

void TcpSocket::OnDataReceive(TCP_HEADER* tcp, uint len)
{
	// 无数据返回ACK
	if (len == 0)
	{
		if (tcp->Flags & (TCP_FLAGS_FIN | TCP_FLAGS_RST))      //FIN结束连接请求标志位。为1表示是结束连接的请求数据包
		{
			SetSeqAck(tcp, 1, true);
			Send(tcp, 0, TCP_FLAGS_ACK);
		}
		else
		{
#if NET_DEBUG
			debug_printf("Tcp Receive(%d) From ", len);
			TinyIP::ShowIP(RemoteIP);
			debug_printf("\r\n");
#endif
		}
		return;
	}

	if(OnReceived)
	{
		// 返回值指示是否向对方发送数据包
		bool rs = OnReceived(this, tcp, tcp->Next(), len);
		if(!rs)
		{
			// 发送ACK，通知已收到
			SetSeqAck(tcp, 1, true);
			Send(tcp, 0, TCP_FLAGS_ACK);
			return;
		}
	}
	else
	{
#if NET_DEBUG
		debug_printf("Tcp Receive(%d) From ", len);
		TinyIP::ShowIP(RemoteIP);
		debug_printf(" : ");
		Sys.ShowString(tcp->Next(), len);
		debug_printf("\r\n");
#endif
	}
	// 发送ACK，通知已收到
	SetSeqAck(tcp, len, true);
	//Send(buf, 0, TCP_FLAGS_ACK);

	//TcpSend(buf, len);

	// 响应Ack和发送数据一步到位
	Send(tcp, len, TCP_FLAGS_ACK | TCP_FLAGS_PUSH);
}

void TcpSocket::OnDisconnect(TCP_HEADER* tcp, uint len)
{
	if(OnDisconnected) OnDisconnected(this, tcp, tcp->Next(), len);

	// RST是对方紧急关闭，这里啥都不干
	if(tcp->Flags & TCP_FLAGS_FIN)
	{
		SetSeqAck(tcp, 1, true);
		//Close(tcp, 0);
		Send(tcp, 0, TCP_FLAGS_ACK | TCP_FLAGS_PUSH | TCP_FLAGS_FIN);
	}
	else
	{
#if NET_DEBUG
		debug_printf("Tcp OnDisconnect "); // 打印发送方的ip
		TinyIP::ShowIP(RemoteIP);
		debug_printf(":%d Flags=0x%02x", RemotePort, tcp->Flags);
		debug_printf("\r\n");
#endif
	}
}

void TcpSocket::Send(TCP_HEADER* tcp, uint len, byte flags)
{
	tcp->SrcPort = __REV16(Port > 0 ? Port : LocalPort);
	tcp->DestPort = __REV16(RemotePort);
    tcp->Flags = flags;
	tcp->WindowSize = __REV16(1024);
	if(tcp->Length < sizeof(TCP_HEADER) / 4) tcp->Length = sizeof(TCP_HEADER) / 4;

	// 必须在校验码之前设置，因为计算校验码需要地址
	Tip->RemoteIP = RemoteIP;

	// 网络序是大端
	tcp->Checksum = 0;
	tcp->Checksum = __REV16(Tip->CheckSum((byte*)tcp, tcp->Size() + len, 2));

	debug_printf("SendTcp: Flags=0x%02x, len=%d(0x%x) %d => %d \r\n", flags, tcp->Length, tcp->Length, __REV16(tcp->SrcPort), __REV16(tcp->DestPort));

	// 注意tcp->Size()包括头部的扩展数据
	Tip->SendIP(IP_TCP, (byte*)tcp, tcp->Size() + len);
}

void TcpSocket::SetSeqAck(TCP_HEADER* tcp, uint ackNum, bool opSeq)
{
    /*
	第一次握手：主机A发送位码为SYN＝1，随机产生Seq=x的数据包到服务器，主机B由SYN=1知道，A要求建立联机
	第二次握手：主机B收到请求后要确认联机信息，向A发送Ack=(A.Seq+1)，SYN=1，ACK=1，随机产生Seq=y的包
	第三次握手：主机A收到后检查Ack是否正确，即A.Seq+1，以及位码ACK是否为1，
	若正确，主机A会再发送Ack=(B.Seq+1)，ACK=1，主机B收到后确认Seq值与ACK=1则连接建立成功。
	完成三次握手，主机A与主机B开始传送数据。

	Seq 序列号。每一个字节都编号，本报文所发送数据的第一个字节的序号。
	Ack 确认号。期望收到对方的下一个报文的数据的第一个字节的序号。
	*/
	//TCP_HEADER* tcp = Header;
	uint ack = tcp->Ack;
	tcp->Ack = __REV(__REV(tcp->Seq) + ackNum);
    if (!opSeq)
    {
		tcp->Seq = __REV(Seq);
    }
	else
	{
		tcp->Seq = ack;
		//tcp->Seq = __REV(Seq);
	}
}

void TcpSocket::SetMss(TCP_HEADER* tcp)
{
	tcp->Length = sizeof(TCP_HEADER) / 4;
    // 头部后面可能有可选数据，Length决定头部总长度（4的倍数）
    //if (mss)
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

void TcpSocket::SendAck(uint len)
{
	TCP_HEADER* tcp = Create();
	tcp->Init(true);
	Send(tcp, len, TCP_FLAGS_ACK | TCP_FLAGS_PUSH);
}

void TcpSocket::Disconnect()
{
	debug_printf("Tcp::Disconnect ");
	Tip->ShowIP(RemoteIP);
	debug_printf(":%d \r\n", RemotePort);

	TCP_HEADER* tcp = Create();
	tcp->Init(true);
	Send(tcp, 0, TCP_FLAGS_ACK | TCP_FLAGS_PUSH | TCP_FLAGS_FIN);
}

void TcpSocket::Send(const byte* buf, uint len)
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

	// 发送的时候采用LocalPort
	LocalPort = BindPort;

	//SetSeqAck(tcp, len, true);
	tcp->Seq = __REV(Seq);
	tcp->Ack = __REV(Ack);
	// 发送数据的时候，需要同时带PUSH和ACK
	Send(tcp, len, TCP_FLAGS_PUSH | TCP_FLAGS_ACK);

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
	tcp->Seq = 0;	// 仅仅是为了Ack=0，tcp->Seq还是会被Socket的顺序Seq替代
	SetSeqAck(tcp, 0, false);
	SetMss(tcp);

	Status = SynSent;
	Send(tcp, 0, TCP_FLAGS_SYN);

	if(Tip->LoopWait(Callback, this, 5000))
	{
		//if(tcp->Flags & TCP_FLAGS_SYN)
		if(Status = SynAck)
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

bool Callback(TinyIP* tip, void* param, MemoryStream& ms)
{
	ETH_HEADER* eth = ms.Retrieve<ETH_HEADER>();
	if(eth->Type != ETH_IP) return false;

	IP_HEADER* _ip = ms.Retrieve<IP_HEADER>();
	if(_ip->Protocol != IP_TCP) return false;

	TcpSocket* socket = (TcpSocket*)param;
	
	// 这里不移动数据流，方便后面调用Process
	TCP_HEADER* tcp = (TCP_HEADER*)_ip->Next();

	// 检查端口
	ushort port = __REV16(tcp->DestPort);
	if(port != socket->Port) return false;

	socket->Header = tcp;

	if(socket->Status == TcpSocket::SynSent)
	{
		if(tcp->Flags & TCP_FLAGS_ACK)
		{
			socket->Status = TcpSocket::SynAck;

			// 处理。如果对方回发第二次握手包，或者终止握手
			//MemoryStream ms(tip->Buffer, tip->BufferSize);
			socket->Process(&ms);

			return true;
		}
	}

	return false;
}

bool TcpSocket::OnWrite(const byte* buf, uint len)
{
	Send(buf, len);
	return len;
}

uint TcpSocket::OnRead(byte* buf, uint len)
{
	// 暂时不支持
	assert_param(false);
	return 0;
}
