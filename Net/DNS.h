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

	IPAddress Query(const String& domain, int msTimeout = 3000);	// 解析
	
private:
	static uint OnReceive(ITransport* port, ByteArray& bs, void* param, void* param2);
	void Process(ByteArray& bs, const IPAddress& server);
	
	ByteArray*	_Buffer;
};

#endif
