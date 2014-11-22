#ifndef __SERVER__H__
#define __SERVER__H__

#include "Sys.h"
#include "Net\ITransport.h"
#include "TinyNet\TinyServer.h"
#include "TinyNet\TinyClient.h"

// 网关服务器
class Gateway
{
private:

public:
	TinyServer* Server;	// 内网服务端
	TinyClient* Client;	// 外网客户端

	Gateway(TinyServer* server, TinyClient* client);
	~Gateway();

	bool Running;
	void Start();
	void Stop();

	// 收到功能消息时触发
	MessageHandler Received;
};

#endif
