#ifndef _SmartOS_DNS_H_
#define _SmartOS_DNS_H_

#include "Net.h"

// DNS协议
class DNS
{
public:
	ISocket*	Socket;

	DNS(ISocket* socket);
	~DNS();

	IPAddress Query(const String& domain, int msTimeout = 2000);	// 解析

private:
	static uint OnReceive(ITransport* port, Array& bs, void* param, void* param2);
	void Process(Array& bs, const IPEndPoint& server);

	ByteArray*	_Buffer;
};

#endif
