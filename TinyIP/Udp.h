#ifndef _TinyIP_UDP_H_
#define _TinyIP_UDP_H_

#include "TinyIP.h"

// Udp会话
class UdpSocket : public Socket
{
public:
	ushort Port;	// 本地端口

	UdpSocket(TinyIP* tip) : Socket(tip)
	{
		Type = IP_UDP;
		Port = 0;
	}

	// 处理数据包
	virtual bool Process(MemoryStream* ms);

	// 收到Udp数据时触发，传递结构体和负载数据长度。返回值指示是否向对方发送数据包
	typedef bool (*UdpHandler)(UdpSocket* socket, UDP_HEADER* udp, byte* buf, uint len);
	UdpHandler OnReceived;

	// 发送UDP数据到目标地址
	void Send(byte* buf, uint len, IPAddress ip, ushort port);
	void Send(byte* buf, uint len, bool checksum = true);

protected:
	virtual void OnReceive(UDP_HEADER* udp, MemoryStream& ms);
};

#endif
