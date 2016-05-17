#ifndef _TinyIP_ICMP_H_
#define _TinyIP_ICMP_H_

#include "Sys.h"
#include "TinyIP.h"

// ICMP协议
class IcmpSocket : public TinySocket
{
public:
	IcmpSocket(TinyIP* tip);

	// 处理数据包
	virtual bool Process(IP_HEADER& ip, Stream& ms);

	// 收到Ping请求时触发，传递结构体和负载数据长度。返回值指示是否向对方发送数据包
	typedef bool (*PingHandler)(IcmpSocket& socket, ICMP_HEADER& icmp, byte* buf, uint len);
	PingHandler OnPing;

	// Ping目的地址，附带a~z重复的负载数据
	bool Ping(IPAddress& ip, uint payloadLength = 32);
};

#endif
