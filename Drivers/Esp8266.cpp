#include "Esp8266.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

/******************************** 内部Tcp/Udp ********************************/
class EspSocket : public ISocket
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
