#include "Gateway.h"

#include "TinyMessage.h"
#include "TokenMessage.h"

bool OnLocalReceived(Message& msg, void* param);
bool OnRemoteReceived(Message& msg, void* param);

// 本地网和远程网一起实例化网关服务
Gateway::Gateway(TinyServer* server, TokenClient* client)
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

// 启动网关。挂载本地和远程的消息事件
void Gateway::Start()
{
	if(Running) return;

	Server->Received = OnLocalReceived;
	Server->Param = this;
	Client->Received = OnRemoteReceived;
	Client->Param = this;

	debug_printf("Gateway::Start \r\n");

	Client->Open();

	Running = true;
}

// 停止网关。取消本地和远程的消息挂载
void Gateway::Stop()
{
	if(!Running) return;

	Server->Received = NULL;
	Server->Param = NULL;
	Client->Received = NULL;
	Client->Param = NULL;

	Running = false;
}

// 本地网收到设备端消息
bool OnLocalReceived(Message& msg, void* param)
{
	Gateway* server = (Gateway*)param;
	assert_ptr(server);

	TinyMessage& msg2 = (TinyMessage&)msg;

	// 消息转发
	if(msg.Code >= 0x10 && msg2.Dest != 0x01)
	{
		debug_printf("Gateway::Local ");
		msg.Show();

		TokenMessage tmsg;
		tmsg.Code = msg.Code;
		// 第一个字节是节点设备地址
		tmsg.Data[0] = ((TinyMessage&)msg).Src;
		if(msg.Length > 0) memcpy(&tmsg.Data[1], msg.Data, msg.Length);
		tmsg.Length = 1 + msg.Length;
		server->Client->Send(tmsg);
	}

	return true;
}

// 远程网收到服务端消息
bool OnRemoteReceived(Message& msg, void* param)
{
	Gateway* server = (Gateway*)param;
	assert_ptr(server);

	// 消息转发
	if(msg.Code >= 0x10)
	{
		debug_printf("Gateway::Remote ");
		msg.Show();

		if(msg.Length <= 0)
		{
			debug_printf("远程网收到的消息应该带有附加数据\r\n");
			return false;
		}

		TinyMessage tmsg;
		tmsg.Code = msg.Code;
		// 第一个字节是节点设备地址
		tmsg.Dest = msg.Data[0];
		if(msg.Length > 1) memcpy(tmsg.Data, &msg.Data[1], msg.Length - 1);
		tmsg.Length = msg.Length - 1;
		server->Server->Send(tmsg);
	}

	return true;
}
