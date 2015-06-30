#include "Gateway.h"

#include "TinyMessage.h"
#include "TokenMessage.h"

bool OnLocalReceived(Message& msg, void* param);
bool OnRemoteReceived(Message& msg, void* param);

void TokenToTiny(TokenMessage& msg, TinyMessage& msg2);
void TinyToToken(TinyMessage& msg, TokenMessage& msg2);

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

	server->OnLocal((TinyMessage&)msg);

	return true;
}

// 远程网收到服务端消息
bool OnRemoteReceived(Message& msg, void* param)
{
	Gateway* server = (Gateway*)param;
	assert_ptr(server);

	server->OnRemote((TokenMessage&)msg);

	return true;
}

// 数据接收中心
void Gateway::OnLocal(TinyMessage& msg)
{
	// 消息转发
	if(msg.Code >= 0x10 && msg.Dest != 0x01)
	{
		debug_printf("Gateway::Local ");
		msg.Show();

		TokenMessage tmsg;
		TinyToToken(msg, tmsg);
		Client->Send(tmsg);
	}
}

void Gateway::OnRemote(TokenMessage& msg)
{
	// 消息转发
	if(msg.Code >= 0x10)
	{
		debug_printf("Gateway::Remote ");
		msg.Show();

		// 需要转发给内网的数据，都至少带有节点ID
		if(msg.Length <= 0)
		{
			debug_printf("远程网收到的消息应该带有附加数据\r\n");
			return;
		}

		TinyMessage tmsg;
		TokenToTiny(msg, tmsg);
		Server->Send(tmsg);
	}
}

// 设备列表 0x21
void Gateway::OnGetDeviceList(Message& msg)
{

}

// 设备信息 x025
void Gateway::OnGetDeviceInfo(Message& msg)
{

}

// 学习模式 0x20
void Gateway::SetMode(bool student)
{
	Student = student;

	TokenMessage msg;
	msg.Code = 0x20;
	msg.Length = 1;
	msg.Data[0] = student ? 1 : 0;

	Client->Send(msg);
}

void Gateway::OnMode(Message& msg)
{
	bool rs = msg.Data[0] != 0;
	SetMode(rs);
}

// 节点注册入网 0x22
void Gateway::DeviceRegister(byte id)
{
	TokenMessage msg;
	msg.Code = 0x22;
	msg.Length = 1;
	msg.Data[0] = id;

	Client->Send(msg);
}

// 节点上线 0x23
void Gateway::DeviceOnline(byte id)
{
	TokenMessage msg;
	msg.Code = 0x23;
	msg.Length = 1;
	msg.Data[0] = id;

	Client->Send(msg);
}

// 节点离线 0x24
void Gateway::DeviceOffline(byte id)
{
	TokenMessage msg;
	msg.Code = 0x24;
	msg.Length = 1;
	msg.Data[0] = id;

	Client->Send(msg);
}

void TokenToTiny(TokenMessage& msg, TinyMessage& msg2)
{
	msg2.Code = msg.Code;
	// 第一个字节是节点设备地址
	msg2.Dest = msg.Data[0];
	if(msg.Length > 1) memcpy(msg2.Data, &msg.Data[1], msg.Length - 1);
	msg2.Length = msg.Length - 1;
}

void TinyToToken(TinyMessage& msg, TokenMessage& msg2)
{
	msg2.Code = msg.Code;
	// 第一个字节是节点设备地址
	msg2.Data[0] = ((TinyMessage&)msg).Src;
	if(msg.Length > 0) memcpy(&msg2.Data[1], msg.Data, msg.Length);
	msg2.Length = 1 + msg.Length;
}

bool operator==(const Device& d1, const Device& d2)
{
	return d1.ID == d2.ID;
}

bool operator!=(const Device& d1, const Device& d2)
{
	return d1.ID != d2.ID;
}
