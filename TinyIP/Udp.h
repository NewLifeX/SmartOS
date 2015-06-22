#ifndef _TinyIP_UDP_H_
#define _TinyIP_UDP_H_

#include "TinyIP.h"

// Udp会话
class UdpSocket : public Socket, public ITransport
{
private:
	//UDP_HEADER* Create();

public:
	ushort 		Port;		// 本地端口，接收该端口数据包。0表示接收所有端口的数据包
	ushort		BindPort;	// 绑定端口，用于发出数据包的源端口。默认为Port，若Port为0，则从1024算起，累加
	IPAddress	RemoteIP;	// 远程地址
	ushort		RemotePort;	// 远程端口
	IPAddress	LocalIP;	// 本地IP地址
	ushort		LocalPort;	// 本地端口，收到数据包的目的端口

	UdpSocket(TinyIP* tip);

	// 处理数据包
	virtual bool Process(IP_HEADER& ip, Stream& ms);

	// 收到Udp数据时触发，传递结构体和负载数据长度。返回值指示是否向对方发送数据包
	typedef bool (*UdpHandler)(UdpSocket& socket, UDP_HEADER& udp, Stream& ms);
	UdpHandler OnReceived;

	// 发送UDP数据到目标地址
	void Send(ByteArray& bs, IPAddress& ip = IPAddress::Any, ushort port = 0);

	virtual string ToString();

protected:
	void Send(UDP_HEADER& udp, uint len, bool checksum = true);
	virtual void OnProcess(IP_HEADER& ip, UDP_HEADER& udp, Stream& ms);

	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const byte* buf, uint len);
	virtual uint OnRead(byte* buf, uint len);
};

#endif
