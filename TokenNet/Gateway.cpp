#include "Time.h"
#include "Gateway.h"
#include "Config.h"

#include "Security\MD5.h"

bool OnLocalReceived(Message& msg, void* param);
bool OnRemoteReceived(Message& msg, void* param);

void TokenToTiny(TokenMessage& msg, TinyMessage& msg2);
void TinyToToken(TinyMessage& msg, TokenMessage& msg2);

// 本地网和远程网一起实例化网关服务
Gateway::Gateway()
{
	Server = NULL;
	Client = NULL;

	Running		= false;
	AutoReport	= false;
}

Gateway::~Gateway()
{
	Stop();

	delete Server;
	Server = NULL;

	delete Client;
	Client = NULL;
}

// 启动网关。挂载本地和远程的消息事件
void Gateway::Start()
{
	if(Running) return;

	assert_param2(Server, "微网服务端未设置");
	assert_param2(Client, "令牌客户端未设置");

	Server->Received	= OnLocalReceived;
	Server->Param		= this;
	Client->Received	= OnRemoteReceived;
	Client->Param		= this;

	debug_printf("Gateway::Start \r\n");

	// 添加网关这一条设备信息
	if(Server->Devices.Count() == 0)
	{
		Device* dv = new Device();
		dv->Address		= Server->Config->Address;
		dv->Kind		= Sys.Code;
		dv->HardID.SetLength(16);
		dv->HardID		= Sys.ID;
		dv->LastTime	= Time.Current();
		dv->Name		= Sys.Name;

		Server->Devices.Add(dv);
	}

	Server->Start();
	Client->Open();

	Running = true;
}

// 停止网关。取消本地和远程的消息挂载
void Gateway::Stop()
{
	if(!Running) return;

	Server->Received	= NULL;
	Server->Param		= NULL;
	Client->Received	= NULL;
	Client->Param		= NULL;

	Running = false;
}

// 本地网收到设备端消息
bool OnLocalReceived(Message& msg, void* param)
{
	Gateway* server = (Gateway*)param;
	assert_ptr(server);

	return server->OnLocal((TinyMessage&)msg);
}

// 远程网收到服务端消息
bool OnRemoteReceived(Message& msg, void* param)
{
	Gateway* server = (Gateway*)param;
	assert_ptr(server);

	return server->OnRemote((TokenMessage&)msg);
}

// 数据接收中心
bool Gateway::OnLocal(TinyMessage& msg)
{
	// 本地处理。注册、上线、同步时间等
	/*switch(msg.Code)
	{
		case 0x01:
			return OnDiscover(msg);
	}*/

	Device* dv = Server->Current;
	if(dv)
	{
		// 短时间内注册或者登录
		ulong now = Time.Current() - 500000;
		if(dv->RegTime > now) DeviceRegister(dv->Address);
		if(dv->LoginTime > now) DeviceOnline(dv->Address);
	}

	// 消息转发
	if(msg.Code >= 0x10 && msg.Dest == 0x01)
	{
		//debug_printf("Gateway::Local ");
		//msg.Show();

		TokenMessage tmsg;
		TinyToToken(msg, tmsg);
		Client->Send(tmsg);
	}

	return true;
}

bool Gateway::OnRemote(TokenMessage& msg)
{
	// 本地处理
	switch(msg.Code)
	{
		case 0x02:
			// 登录以后自动发送设备列表和设备信息
			if(AutoReport && msg.Reply && Client->Token != 0)
			{
				TokenMessage msg;
				msg.Code = 0x21;
				OnGetDeviceList(msg);

				//SendDeviceInfo(dv);
				for(int i=0; i<Server->Devices.Count(); i++)
					SendDeviceInfo(Server->Devices[i]);
			}
			break;

		case 0x20:
			return OnMode(msg);
		case 0x21:
			return OnGetDeviceList(msg);
		case 0x25:
			return OnGetDeviceInfo(msg);
	    case 0x26:
           OnDeviceDelete(msg);
		   break;
	}

	// 消息转发
	if(msg.Code >= 0x10)
	{
		debug_printf("Gateway::Remote ");
		msg.Show();

		// 需要转发给内网的数据，都至少带有节点ID
	/*	if(msg.Length <= 0)
		{
			debug_printf("远程网收到的消息应该带有附加数据\r\n");
			return false;
		}
		*/
		TinyMessage tmsg;
		TokenToTiny(msg, tmsg);
		bool rs = Server->Dispatch(tmsg);
		if(rs)
		{
			TinyToToken(tmsg, msg);
			return Client->Reply(msg);
		}
	}

	return true;
}

// 设备列表 0x21
bool Gateway::OnGetDeviceList(Message& msg)
{
	if(msg.Reply) return false;

	// 不管请求内容是什么，都返回设备ID列表
	TokenMessage rs;
	rs.Code = msg.Code;

	if(Server->Devices.Count() == 0)
	{
		rs.Data[0] = 0;
		rs.Length = 1;
	}
	else
	{
		int i = 0;
		for(i=0; i<Server->Devices.Count(); i++)
			rs.Data[i] = Server->Devices[i]->Address;
		rs.Length = i;
	}

    debug_printf("获取设备列表 共%d个\r\n", Server->Devices.Count());

	return Client->Reply(rs);
}

// 设备信息 0x25
bool Gateway::OnGetDeviceInfo(Message& msg)
{
	if(msg.Reply) return false;

	// 如果没有给ID，那么返回空负载
	if(msg.Length == 0) return Client->Reply((TokenMessage&)msg);

	TokenMessage rs;
	rs.Code = msg.Code;

	// 如果未指定设备ID，则默认为1，表示网关自身
	byte id = 1;
	if(msg.Length > 0) id = msg.Data[0];
	debug_printf("获取节点信息 ID=0x%02X\r\n", id);

	// 找到对应该ID的设备
	Device* dv = Server->FindDevice(id);

	// 即使找不到设备，也返回空负载数据
	if(!dv) return Client->Reply(rs);

	dv->Show(true);

	return SendDeviceInfo(dv);
}

// 发送设备信息
bool Gateway::SendDeviceInfo(Device* dv)
{
	if(!dv) return false;

	TokenMessage rs;
	rs.Code = 0x25;
	// 担心rs.Data内部默认缓冲区不够大，这里直接使用数据流。必须小心，ms生命结束以后，它的缓冲区也将无法使用
	//Stream ms(rs.Data, rs.Length);
	Stream ms;

	dv->Write(ms);

	// 当前作用域，直接使用数据流的指针，内容可能扩容而导致指针改变
	rs.Data = ms.GetBuffer();
	rs.Length = ms.Position();

	return Client->Reply(rs);
}

// 学习模式 0x20
void Gateway::SetMode(bool student)
{
	Student = student;

	TokenMessage msg;
	msg.Code = 0x20;
	msg.Length = 1;
	msg.Data[0] = student ? 1 : 0;
	debug_printf("学习模式\r\n");

	Client->Send(msg);
}

bool Gateway::OnMode(Message& msg)
{
	bool rs = msg.Data[0] != 0;
	SetMode(rs);

	return true;
}

// 节点注册入网 0x22
void Gateway::DeviceRegister(byte id)
{
	TokenMessage msg;
	msg.Code = 0x22;
	msg.Length = 1;
	msg.Data[0] = id;
	debug_printf("节点注册入网 ID=0x%02X\r\n", id);

	Client->Send(msg);
	//if(AutoReport)
	//{
	//	Device* dv = Server->FindDevice(id);
	//	SendDeviceInfo(dv);
	//}
}

// 节点上线 0x23
void Gateway::DeviceOnline(byte id)
{
	TokenMessage msg;
	msg.Code = 0x23;
	msg.Length = 1;
	msg.Data[0] = id;
	debug_printf("节点上线 ID=0x%02X\r\n", id);

	Client->Send(msg);
	//if(AutoReport)
	//{
	//	Device* dv = Server->FindDevice(id);
	//	SendDeviceInfo(dv);
	//}
}

// 节点离线 0x24
void Gateway::DeviceOffline(byte id)
{
	TokenMessage msg;
	msg.Code = 0x24;
	msg.Length = 1;
	msg.Data[0] = id;
	debug_printf("节点离线 ID=0x%02X\r\n", id);

	Client->Send(msg);
	if(AutoReport)
	{
		Device* dv = Server->FindDevice(id);
		SendDeviceInfo(dv);
	}
}

// 节点删除 0x26
void Gateway::OnDeviceDelete(Message& msg)
{
	if(msg.Reply) return;
	if(msg.Length == 0) return;

	byte id = msg.Data[0];

	TokenMessage rs;
	rs.Code = msg.Code;

	debug_printf("节点删除 ID=0x%02X\r\n", id);

	bool success = Server->DeleteDevice(id);

	rs.Length = 1;
	rs.Data[0] = success ? 0 : 1;

	Client->Reply(rs);

}

void TokenToTiny(TokenMessage& msg, TinyMessage& msg2)
{
	
	msg2.Code = msg.Code;

	// 处理Reply标记
	msg2.Reply = msg.Reply;
	msg2.Error = msg.Error;

	// 第一个字节是节点设备地址
	if(msg.Length > 0) msg2.Dest = msg.Data[0];

	if(msg.Length > 1) memcpy(msg2.Data, &msg.Data[1], msg.Length - 1);

	msg2.Length = msg.Length - 1;
}

void TinyToToken(TinyMessage& msg, TokenMessage& msg2)
{
	msg2.Code = msg.Code;

	// 处理Reply标记
	msg2.Reply = msg.Reply;
	msg2.Error = msg.Error;

	// 第一个字节是节点设备地址
	msg2.Data[0] = ((TinyMessage&)msg).Src;

	if(msg.Length > 0) memcpy(&msg2.Data[1], msg.Data, msg.Length);

	msg2.Length = 1 + msg.Length;
}
