#ifndef _SmartOS_DNS_H_
#define _SmartOS_DNS_H_

#include "Net\ITransport.h"

// DNS协议
class DNS
{
public:
	NetworkInterface&	Host;	// 主机

	DNS(NetworkInterface& host, const IPAddress& dns);
	~DNS();

	IPAddress Query(const String& domain, int msTimeout = 1000);	// 解析

	// 快捷查询。借助主机直接查询多次
	static IPAddress Query(NetworkInterface& host, const String& domain, int times = 5, int msTimeout = 1000);

private:
	static uint OnReceive(ITransport* port, Buffer& bs, void* param, void* param2);
	void Process(Buffer& bs, const IPEndPoint& server);

	Socket*		_Socket;
	Array*		_Buffer;
};

#endif
