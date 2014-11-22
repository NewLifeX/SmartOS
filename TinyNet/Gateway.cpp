#include "Gateway.h"

bool OnLocalReceived(Message& msg, void* param);
bool OnRemoteReceived(Message& msg, void* param);

Gateway::Gateway(TinyServer* server, TinyClient* client)
{
	assert_param(server);
	assert_param(client);

	Server = server;
	Client = client;

	Running = false;
}

Gateway::~Gateway()
{
	delete Server;
	Server = NULL;

	delete Client;
	Client = NULL;
}

void Gateway::Start()
{
	if(Running) return;

	Server->Received = OnLocalReceived;
	Server->Param = this;
	Client->Received = OnRemoteReceived;
	Client->Param = this;

	debug_printf("Gateway::Start \r\n");

	Running = true;
}

void Gateway::Stop()
{
	if(!Running) return;

	Server->Received = NULL;
	Server->Param = NULL;
	Client->Received = NULL;
	Client->Param = NULL;

	Running = false;
}

bool OnLocalReceived(Message& msg, void* param)
{
	Gateway* server = (Gateway*)param;
	assert_ptr(server);

	// 消息转发
	if(msg.Code >= 0x10)
	{
		debug_printf("Gateway::Local %d \r\n", msg.Src);
		server->Client->Send(msg);
	}

	return true;
}

bool OnRemoteReceived(Message& msg, void* param)
{
	Gateway* server = (Gateway*)param;
	assert_ptr(server);

	// 消息转发
	if(msg.Code >= 0x10)
	{
		debug_printf("Gateway::Remote %d \r\n", msg.Src);
		server->Server->Send(msg);
	}

	return true;
}
