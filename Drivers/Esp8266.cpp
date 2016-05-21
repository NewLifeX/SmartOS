#include "Message\DataStore.h"
#include "Esp8266.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

/******************************** 内部Tcp/Udp ********************************/
class EspSocket : public Object, public ITransport, public ISocket
{
private:
	Esp8266&	_Host;

public:
	EspSocket(Esp8266& host, ProtocolType protocol);
	virtual ~EspSocket();

	// 应用配置，修改远程地址和端口
	void Change(const IPEndPoint& remote);

	virtual bool OnWrite(const Buffer& bs);
	virtual uint OnRead(Buffer& bs);

	// 发送数据
	virtual bool Send(const Buffer& bs);
	// 接收数据
	virtual uint Receive(Buffer& bs);
	
};

class EspTcp : public EspSocket
{
public:
	EspTcp(Esp8266& host);
};

class EspUdp : public EspSocket
{
public:
	EspUdp(Esp8266& host);

	virtual bool SendTo(const Buffer& bs, const IPEndPoint& remote);

	// 中断分发  维护状态
	virtual void OnProcess(byte reg);
	// 用户注册的中断事件处理 异步调用
	virtual void RaiseReceive();

	virtual String& ToStr(String& str) const { return str + "Udp_" + Local.Port; }

private:
	virtual bool OnWriteEx(const Buffer& bs, const void* opt);
};

/******************************** Esp8266 ********************************/

Esp8266::Esp8266(ITransport* port, Pin rst)
{
	Set(port);
    
	if(rst != P0) _rst.Set(rst);
}

bool Esp8266::OnOpen()
{
	if(!PackPort::OnOpen()) return false;

	if(!_rst.Empty())
	{
		_rst.Open();

		_rst = true;
		Sys.Delay(100);
		_rst = false;
		Sys.Delay(100);
		_rst = true;
	}

	return true;
}

void Esp8266::OnClose()
{
	_rst.Close();

	PackPort::OnClose();
}

// 配置网络参数
void Esp8266::Config()
{
	
}

ISocket* Esp8266::CreateSocket(ProtocolType type)
{
	switch(type)
	{
		case ProtocolType::Tcp:
			return new EspTcp(*this);

		case ProtocolType::Udp:
			return new EspUdp(*this);

		default:
			return nullptr;
	}
}

/******************************** Socket ********************************/

EspSocket::EspSocket(Esp8266& host, ProtocolType protocol)
	: _Host(host)
{
	Host		= &host;
	Protocol	= protocol;
}

EspSocket::~EspSocket()
{
	Close();
}

// 应用配置，修改远程地址和端口
void EspSocket::Change(const IPEndPoint& remote)
{
#if DEBUG
	/*debug_printf("%s::Open ", Protocol == 0x01 ? "Tcp" : "Udp");
	Local.Show(false);
	debug_printf(" => ");
	remote.Show(true);*/
#endif

	// 设置端口目的(远程)IP地址
	/*SocRegWrites(DIPR, remote.Address.ToArray());
	// 设置端口目的(远程)端口号
	SocRegWrite2(DPORT, _REV16(remote.Port));*/
}

// 接收数据
uint EspSocket::Receive(Buffer& bs)
{
	if(!Open()) return 0;

	/*// 读取收到数据容量
	ushort size = _REV16(SocRegRead2(RX_RSR));
	if(size == 0)
	{
		// 没有收到数据时，需要给缓冲区置零，否则系统逻辑会混乱
		bs.SetLength(0);
		return 0;
	}

	// 读取收到数据的首地址
	ushort offset = _REV16(SocRegRead2(RX_RD));

	// 长度受 bs 限制时 最大读取bs.Lenth
	if(size > bs.Length()) size = bs.Length();

	// 设置 实际要读的长度
	bs.SetLength(size);

	_Host.ReadFrame(offset, bs, Index, 0x03);

	// 更新实际物理地址,
	SocRegWrite2(RX_RD, _REV16(offset + size));
	// 生效 RX_RD
	WriteConfig(RECV);

	// 等待操作完成
	// while(ReadConfig());

	//返回接收到数据的长度
	return size;*/
	return 0;
}

// 发送数据
bool EspSocket::Send(const Buffer& bs)
{
	if(!Open()) return false;
	/*debug_printf("%s::Send [%d]=", Protocol == 0x01 ? "Tcp" : "Udp", bs.Length());
	bs.Show(true);*/

	/*// 读取状态
	byte st = ReadStatus();
	// 不在UDP  不在TCP连接OK 状态下返回
	if(!(st == SOCK_UDP || st == SOCK_ESTABLISHE))return false;
	// 读取缓冲区空闲大小 硬件内部自动计算好空闲大小
	ushort remain = _REV16(SocRegRead2(TX_FSR));
	if( remain < bs.Length())return false;

	// 读取发送缓冲区写指针
	ushort addr = _REV16(SocRegRead2(TX_WR));
	_Host.WriteFrame(addr, bs, Index, 0x02);
	// 更新发送缓存写指针位置
	addr += bs.Length();
	SocRegWrite2(TX_WR,_REV16(addr));

	// 启动发送 异步中断处理发送异常等
	WriteConfig(SEND);

	// 控制轮询任务，加快处理
	Sys.SetTask(_Host.TaskID, true, 20);*/

	return true;
}

bool EspSocket::OnWrite(const Buffer& bs) {	return Send(bs); }
uint EspSocket::OnRead(Buffer& bs) { return Receive(bs); }

/******************************** Tcp ********************************/

EspTcp::EspTcp(Esp8266& host)
	: EspSocket(host, ProtocolType::Tcp)
{
	
}

/******************************** Udp ********************************/

EspUdp::EspUdp(Esp8266& host)
	: EspSocket(host, ProtocolType::Udp)
{
	
}

bool EspUdp::SendTo(const Buffer& bs, const IPEndPoint& remote)
{
	if(remote == Remote) return Send(bs);

	Change(remote);
	bool rs = Send(bs);
	Change(Remote);

	return rs;
}

bool EspUdp::OnWriteEx(const Buffer& bs, const void* opt)
{
	auto ep = (IPEndPoint*)opt;
	if(!ep) return OnWrite(bs);

	return SendTo(bs, *ep);
}

void EspUdp::OnProcess(byte reg)
{
	/*S_Interrupt ir;
	ir.Init(reg);
	// UDP 模式下只处理 SendOK  Recv 两种情况

	if(ir.RECV)
	{
		RaiseReceive();
	}*/
	//	SEND OK   不需要处理 但要清空中断位
}

// UDP 异步只有一种情况  收到数据  可能有多个数据包
// UDP接收到的数据结构： RemoteIP(4 byte) + RemotePort(2 byte) + Length(2 byte) + Data(Length byte)
void EspUdp::RaiseReceive()
{
	/*byte buf[1500];
	Buffer bs(buf, ArrayLength(buf));
	ushort size = Receive(bs);
	Stream ms(bs.GetBuffer(), size);

	// 拆包
	while(ms.Remain())
	{
		IPEndPoint ep	= ms.ReadArray(6);
		ep.Port = _REV16(ep.Port);

		ushort len = ms.ReadUInt16();
		len = _REV16(len);
		// 数据长度不对可能是数据错位引起的，直接丢弃数据包
		if(len > 1500)
		{
			net_printf("W5500 UDP数据接收有误, ep=%s Length=%d \r\n", ep.ToString().GetBuffer(), len);
			return;
		}
		// 回调中断
		Buffer bs3(ms.ReadBytes(len), len);
		OnReceive(bs3, &ep);
	};*/
}
