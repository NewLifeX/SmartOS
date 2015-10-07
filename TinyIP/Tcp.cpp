#include "Time.h"
#include "Tcp.h"

#define NET_DEBUG 0
//#define NET_DEBUG DEBUG

bool* WaitAck;

bool Callback(TinyIP* tip, void* param, Stream& ms);

TcpSocket::TcpSocket(TinyIP* tip) : TinySocket(tip, IP_TCP)
{
	MaxSize	= 1500;
	Host	= tip;

	// 累加端口
	static ushort g_tcp_port = 1024;
	if(g_tcp_port < 1024) g_tcp_port = 1024;
	Local.Port = g_tcp_port++;
	Local.Address = tip->IP;

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
	sprintf(name, "TCP_%d", Local.Port);
	return name;
}

bool TcpSocket::OnOpen()
{
	debug_printf("Tcp::Open %d\r\n", Local.Port);

	Enable = true;
	return Enable;
}

void TcpSocket::OnClose()
{
	debug_printf("Tcp::Close %d\r\n", Local.Port);
	Enable = false;

	Status = Closed;
}

bool TcpSocket::Process(IP_HEADER& ip, Stream& ms)
{
	TCP_HEADER* tcp = ms.Retrieve<TCP_HEADER>();
	if(!tcp) return false;

	Header = tcp;
	uint len = ms.Remain();

	ushort port = __REV16(tcp->DestPort);
	ushort remotePort = __REV16(tcp->SrcPort);

	// 仅处理本连接的IP和端口
	if(port != Local.Port) return false;

	//Local.Port		= port;
	//Local.Address	= ip.DestIP;

	if(WaitAck && (tcp->Flags & TCP_FLAGS_ACK))
	{
		*WaitAck = true;
	}

	OnProcess(*tcp, ms);

	return true;
}

void TcpSocket::OnProcess(TCP_HEADER& tcp, Stream& ms)
{
	// 计算标称的数据长度
	//uint len = tcp.Size() - sizeof(TCP_HEADER);
	// TCP好像没有标识数据长度的字段，但是IP里面有，这样子的话，ms里面的长度是准确的
	uint len = ms.Remain();

	uint seq = __REV(tcp.Seq);
	uint ack = __REV(tcp.Ack);

#if NET_DEBUG
	debug_printf("Tcp::Process Flags=0x%02x Seq=0x%04x Ack=0x%04x From ", tcp.Flags, seq, ack);
	Remote.Show();
	debug_printf("\r\n");
#endif

	// 下次主动发数据时，用该序列号，因为对方Ack确认期望下次得到这个序列号
	Seq = ack;
	Ack = seq + len;
	//debug_printf("Seq=0x%04x Ack=0x%04x \r\n", Seq, Ack);

	// 第一次同步应答
	if (tcp.Flags & TCP_FLAGS_SYN) // SYN连接请求标志位，为1表示发起连接的请求数据包
	{
		Ack++;	// 此时加一
		if(!(tcp.Flags & TCP_FLAGS_ACK))
			OnAccept(tcp, len);
		else
			OnAccept3(tcp, len);
	}
	else if(tcp.Flags & (TCP_FLAGS_FIN | TCP_FLAGS_RST))
	{
		Ack++;	// 此时加一
		OnDisconnect(tcp, len);
	}
	// 第三次同步应答,三次应答后方可传输数据
	else if (tcp.Flags & TCP_FLAGS_ACK) // ACK确认标志位，为1表示此数据包为应答数据包
	{
		if(len == 0 && tcp.Ack <= 1)
			OnAccept3(tcp, len);
		else
			OnDataReceive(tcp, len);
	}
	else
		debug_printf("Tcp::Process 未知标识位 0x%02x \r\n", tcp.Flags);
}

// 服务端收到握手二，也是首次收到来自客户端的数据
void TcpSocket::OnAccept(TCP_HEADER& tcp, uint len)
{
	if(Status == Closed) Status = SynSent;

	if(OnAccepted)
		OnAccepted(*this, tcp, tcp.Next(), len);
	else
	{
#if NET_DEBUG
		debug_printf("Tcp:Accept "); // 打印发送方的ip
		Remote.Show();
		debug_printf("\r\n");
#endif
	}

	//第二次同步应答
	SetSeqAck(tcp, 1, false);
	SetMss(tcp);

	// 需要用到MSS，所以采用4个字节的可选段
	//SendPacket(tcp, 4, TCP_FLAGS_SYN | TCP_FLAGS_ACK);
	// 注意tcp->Size()包括头部的扩展数据，这里不用单独填4
	SendPacket(tcp, 0, TCP_FLAGS_SYN | TCP_FLAGS_ACK);
}

// 客户端收到握手三，也是首次收到来自服务端的数据，或者0数据的ACK
void TcpSocket::OnAccept3(TCP_HEADER& tcp, uint len)
{
	if(Status == SynSent) Status = Established;

	if(OnAccepted)
		OnAccepted(*this, tcp, tcp.Next(), len);
	else
	{
#if NET_DEBUG
		debug_printf("Tcp:Accept3 "); // 打印发送方的ip
		Remote.Show();
		debug_printf("\r\n");
#endif
	}

	// 第二次同步应答
	SetSeqAck(tcp, 1, true);
	// 不需要Mss
	tcp.Length = sizeof(TCP_HEADER) / 4;

	SendPacket(tcp, 0, TCP_FLAGS_ACK);
}

void TcpSocket::OnDataReceive(TCP_HEADER& tcp, uint len)
{
	// 无数据返回ACK
	if (len == 0)
	{
		if (tcp.Flags & (TCP_FLAGS_FIN | TCP_FLAGS_RST))      //FIN结束连接请求标志位。为1表示是结束连接的请求数据包
		{
			SetSeqAck(tcp, 1, true);
			SendPacket(tcp, 0, TCP_FLAGS_ACK);
		}
		else
		{
#if NET_DEBUG
			debug_printf("Tcp:Receive(%d) From ", len);
			Remote.Show();
			debug_printf("\r\n");
#endif
		}
		return;
	}

	byte* data = tcp.Next();

	// 触发ITransport接口事件
	ByteArray bs(data, len);
	uint len2 = OnReceive(bs, NULL);
	// 如果有返回，说明有数据要回复出去
	if(len2)
	{
		// 发送ACK，通知已收到
		SetSeqAck(tcp, len, true);

		// 响应Ack和发送数据一步到位
		SendPacket(tcp, len2, TCP_FLAGS_ACK | TCP_FLAGS_PUSH);
	}

	if(OnReceived)
	{
		// 返回值指示是否向对方发送数据包
		bool rs = OnReceived(*this, tcp, data, len);
		// 如果不需要向对方发数据包，则直接响应ACK
		if(!rs)
		{
			// 发送ACK，通知已收到
			SetSeqAck(tcp, len, true);
			SendPacket(tcp, 0, TCP_FLAGS_ACK);
			return;
		}
	}
	else
	{
#if NET_DEBUG
		debug_printf("Tcp:Receive(%d) From ", len);
		Remote.Show();
		debug_printf(" ");
		Sys.ShowString(data, len);
		debug_printf("\r\n");
#endif
	}
	// 发送ACK，通知已收到
	SetSeqAck(tcp, len, true);
	SendPacket(tcp, 0, TCP_FLAGS_ACK);

	// 响应Ack和发送数据一步到位
	//SendPacket(tcp, len, TCP_FLAGS_ACK | TCP_FLAGS_PUSH);
}

void TcpSocket::OnDisconnect(TCP_HEADER& tcp, uint len)
{
	Status = Closed;

	if(OnDisconnected) OnDisconnected(*this, tcp, tcp.Next(), len);

	// RST是对方紧急关闭，这里啥都不干
	if(tcp.Flags & TCP_FLAGS_FIN)
	{
		SetSeqAck(tcp, 1, true);
		//Close(tcp, 0);
		SendPacket(tcp, 0, TCP_FLAGS_ACK | TCP_FLAGS_PUSH | TCP_FLAGS_FIN);
	}
	else if(!OnDisconnected)
	{
#if NET_DEBUG
		debug_printf("Tcp:OnDisconnect "); // 打印发送方的ip
		Remote.Show();
		debug_printf(" Flags=0x%02x", tcp.Flags);
		debug_printf("\r\n");
#endif
	}

	Status = Closed;
}

bool TcpSocket::SendPacket(TCP_HEADER& tcp, uint len, byte flags)
{
	tcp.SrcPort = __REV16(Local.Port);
	tcp.DestPort = __REV16(Remote.Port);
    tcp.Flags = flags;
	tcp.WindowSize = __REV16(1024);
	if(tcp.Length < sizeof(TCP_HEADER) / 4) tcp.Length = sizeof(TCP_HEADER) / 4;

	// 必须在校验码之前设置，因为计算校验码需要地址
	//Tip->RemoteIP = RemoteIP;

	// 网络序是大端
	tcp.Checksum = 0;
	tcp.Checksum = __REV16(Tip->CheckSum(&Remote.Address, (byte*)&tcp, tcp.Size() + len, 2));

#if NET_DEBUG
	uint hlen = tcp.Length << 2;
	debug_printf("SendTcp: Flags=0x%02x Seq=0x%04x Ack=0x%04x Length=%d(0x%x) Payload=%d(0x%x) %d => %d \r\n", flags, __REV(tcp.Seq), __REV(tcp.Ack), hlen, hlen, len, len, __REV16(tcp.SrcPort), __REV16(tcp.DestPort));
#endif

	// 注意tcp->Size()包括头部的扩展数据
	return Tip->SendIP(IP_TCP, Remote.Address, (byte*)&tcp, tcp.Size() + len);
}

void TcpSocket::SetSeqAck(TCP_HEADER& tcp, uint ackNum, bool opSeq)
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
	uint ack = tcp.Ack;
	tcp.Ack = __REV(__REV(tcp.Seq) + ackNum);
    if (!opSeq)
    {
		tcp.Seq = __REV(Seq);
    }
	else
	{
		tcp.Seq = ack;
		//tcp.Seq = __REV(Seq);
	}
}

void TcpSocket::SetMss(TCP_HEADER& tcp)
{
	tcp.Length = sizeof(TCP_HEADER) / 4;
    // 头部后面可能有可选数据，Length决定头部总长度（4的倍数）
    //if (mss)
    {
		uint* p = (uint*)tcp.Next();
        // 使用可选域设置 MSS 到 1460:0x5b4
		p[0] = __REV(0x020405b4);
		p[1] = __REV(0x01030302);
		p[2] = __REV(0x01010402);

		tcp.Length += 3;
    }
}

void TcpSocket::SendAck(uint len)
{
	Stream ms(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(TCP_HEADER) + len);
	ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER));

	TCP_HEADER* tcp = ms.Retrieve<TCP_HEADER>();
	tcp->Init(true);
	SendPacket(*tcp, len, TCP_FLAGS_ACK | TCP_FLAGS_PUSH);
}

bool TcpSocket::Disconnect()
{
	debug_printf("Tcp::Disconnect ");
	Remote.Show();
	debug_printf("\r\n");

	Stream ms(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(TCP_HEADER));
	ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER));

	TCP_HEADER* tcp = ms.Retrieve<TCP_HEADER>();
	tcp->Init(true);
	return SendPacket(*tcp, 0, TCP_FLAGS_ACK | TCP_FLAGS_PUSH | TCP_FLAGS_FIN);
}

bool TcpSocket::Send(const ByteArray& bs)
{
	if(!Enable)
	{
		if(!Open()) return false;
	}

	// 如果连接已关闭，必须重新连接
	if(Status == Closed)
	{
		if(!Connect(Remote.Address, Remote.Port)) return false;
	}

#if NET_DEBUG
	debug_printf("Tcp::Send ");
	Remote.Show();
	debug_printf(" buf=0x%08x len=%d ...... \r\n", bs.GetBuffer(), bs.Length());
#endif

	//Stream ms(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(TCP_HEADER) + bs.Length());
	//byte buf[sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(TCP_HEADER) + 256];
	byte buf[1500];
	Stream ms(buf, ArrayLength(buf));
	ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER));

	TCP_HEADER* tcp = ms.Retrieve<TCP_HEADER>();
	tcp->Init(true);

	// 复制数据，确保数据不会溢出
	ms.Write(bs);

	//SetSeqAck(tcp, len, true);
	tcp->Seq = __REV(Seq);
	tcp->Ack = __REV(Ack);
	// 发送数据的时候，需要同时带PUSH和ACK
	//debug_printf("Seq=0x%04x Ack=0x%04x \r\n", Seq, Ack);
	if(!SendPacket(*tcp, bs.Length(), TCP_FLAGS_PUSH | TCP_FLAGS_ACK)) return false;

	bool wait = false;
	WaitAck = &wait;

	// 等待响应
	TimeWheel tw(0, 3000);
	tw.Sleep = 1;
	do{
		if(wait) break;
	}while(!tw.Expired());

	WaitAck = NULL;

#if NET_DEBUG
	if(wait)
		debug_printf("发送成功！\r\n");
	else
		debug_printf("发送失败！\r\n");
#endif

	return wait;
}

uint TcpSocket::Receive(ByteArray& bs)
{
	return 0;
}

// 连接远程服务器，记录远程服务器IP和端口，后续发送数据和关闭连接需要
bool TcpSocket::Connect(IPAddress& ip, ushort port)
{
	if(ip.IsAny() || port == 0) return false;

	if(!Enable)
	{
		if(!Open()) return false;
	}

	Remote.Address	= ip;
	Remote.Port		= port;

	debug_printf("Tcp::Connect ");
	Local.Show();
	debug_printf(" => ");
	Remote.Show();
	debug_printf(" ...... \r\n");

	Stream ms(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(TCP_HEADER) + 3);
	ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER));

	TCP_HEADER* tcp = ms.Retrieve<TCP_HEADER>();
	tcp->Init(true);
	//tcp->Seq = 0;	// 仅仅是为了Ack=0，tcp->Seq还是会被Socket的顺序Seq替代
	//SetSeqAck(tcp, 0, false);
	tcp->Seq = __REV(Seq);
	tcp->Ack = 0;
	SetMss(*tcp);

	Status = SynSent;
	if(!SendPacket(*tcp, 0, TCP_FLAGS_SYN))
	{
		Status = Closed;
		return false;
	}

	bool wait = false;
	WaitAck = &wait;

	// 等待响应
	TimeWheel tw(0, 3000);
	tw.Sleep = 1;
	do{
		if(wait) break;
	}while(!tw.Expired());

	WaitAck = NULL;

	if(wait)
	{
		if(Status == Established)
		{
			//Status = Established;
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

bool TcpSocket::OnWrite(const ByteArray& bs)
{
	return Send(bs);
}

uint TcpSocket::OnRead(ByteArray& bs)
{
	// 暂时不支持
	return 0;
}

/*
三次握手过程：
A=>B SYN
B=>A SYN+ACK
A=>B ACK
*/
