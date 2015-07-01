#include "Gateway.h"

#include "TinyMessage.h"
#include "TokenMessage.h"

#include "Security\MD5.h"

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

	Server->Received	= OnLocalReceived;
	Server->Param		= this;
	Client->Received	= OnRemoteReceived;
	Client->Param		= this;

	debug_printf("Gateway::Start \r\n");

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
	switch(msg.Code)
	{
		case 0x01:
			return OnDiscover(msg);
	}

	// 临时方便调试使用
	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	Device* dv = FindDevice(id);
	if(!dv && id)
	{
		dv = new Device();
		dv->ID = id;

		Devices.Add(dv);

		// 模拟注册
		DeviceRegister(id);

		// 模拟上线
		DeviceOnline(id);
	}
	// 更新设备信息
	if(dv)
	{
		dv->LastTime = Time.Current();
	}

	// 消息转发
	//if(msg.Code >= 0x10 && msg.Dest != 0x01)
	// 调试时所有指令上报云端
	if(msg.Code >= 0x10)
	{
		debug_printf("Gateway::Local ");
		msg.Show();

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
		case 0x20:
			return OnMode(msg);
		case 0x21:
			return OnGetDeviceList(msg);
		case 0x25:
			return OnGetDeviceInfo(msg);
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
		Server->Send(tmsg);
	}

	return true;
}

// 设备列表 0x21
bool Gateway::OnGetDeviceList(Message& msg)
{
	// 不管请求内容是什么，都返回设备ID列表
	TokenMessage rs;
	rs.Code = msg.Code;

	if(Devices.Count()==0)
	{
		rs.Data[0]=0;
		rs.Length=1;
	}
	else
	{
	   int i = 0;
	   for(i=0; i<Devices.Count(); i++)
		 rs.Data[i] = Devices[i]->ID;
	   rs.Length = i;
	}

    debug_printf(" 获取设备列表\r\n");
	return Client->Reply(rs);
}

// 设备信息 x025
bool Gateway::OnGetDeviceInfo(Message& msg)
{
	TokenMessage rs;
	rs.Code = msg.Code;

	// 如果未指定设备ID，则默认为1，表示网关自身
	byte id = 1;
	if(rs.Length > 0) id = msg.Data[0];
	debug_printf("获取节点信息\r\n");

	// 找到对应该ID的设备
	Device* dv = FindDevice(id);

	// 即使找不到设备，也返回空负载数据
	if(dv)
	{
		Stream ms(rs.Data, rs.Length);

		dv->Write(ms);

		// 当前作用域，直接使用数据流的指针，内容可能扩容而导致指针改变
		rs.Data = ms.GetBuffer();
		rs.Length = ms.Position();

		return Client->Reply(rs);
	}
	else
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
	debug_printf("节点注册入网\r\n");

	Client->Send(msg);
}

// 节点上线 0x23
void Gateway::DeviceOnline(byte id)
{
	TokenMessage msg;
	msg.Code = 0x23;
	msg.Length = 1;
	msg.Data[0] = id;
	debug_printf("节点上线\r\n");

	Client->Send(msg);
}

// 节点离线 0x24
void Gateway::DeviceOffline(byte id)
{
	TokenMessage msg;
	msg.Code = 0x24;
	msg.Length = 1;
	msg.Data[0] = id;
	debug_printf("节点离线\r\n");

	Client->Send(msg);
}

Device* Gateway::FindDevice(byte id)
{
	if(id == 0) return NULL;

	for(int i=0; i<Devices.Count(); i++)
	{
		if(id == Devices[i]->ID) return Devices[i];
	}

	return NULL;
}

// 设备发现
// 请求：2字节设备类型 + 20字节系统ID
// 响应：1字节地址 + 8字节密码
bool Gateway::OnDiscover(TinyMessage& msg)
{
	if(!msg.Reply)
	{
		// 如果设备列表没有这个设备，那么加进去
		byte id = msg.Src;
		Device* dv = FindDevice(id);
		if(!dv && id)
		{
			Stream ms(msg.Data, msg.Length);

			dv = new Device();
			dv->ID = id;
			dv->Type = ms.Read<ushort>();
			ms.ReadArray(dv->HardID);

			Devices.Add(dv);

			debug_printf("Gateway::Discover ID=0x%02X Type=%04X HardID=", id, dv->Type);
			dv->HardID.Show();
			debug_printf("\r\n");

			// 节点注册
			DeviceRegister(id);
		}
		// 更新设备信息
		if(dv)
		{
			// 如果最后活跃时间超过60秒，则认为是设备上线
			if(dv->LastTime + 60000000 < Time.Current())
			{
				// 节点上线
				DeviceOnline(id);
			}
			dv->LastTime = Time.Current();

			// 生成随机密码。当前时间的MD5
			ulong now = Time.Current();
			ByteArray bs((byte*)&now, 8);
			MD5::Hash(bs, dv->Pass);

			// 响应
			Stream ms(msg.Data, msg.Length);
			ms.Write(id);
			ms.WriteArray(dv->Pass);

			Server->Reply(msg);
		}
	}

	return true;
}


void TokenToTiny(TokenMessage& msg, TinyMessage& msg2)
{
	msg2.Code = msg.Code;
	if(msg.Length>0)
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

void Device::Write(Stream& ms)
{
	ms.Write(ID);
	ms.Write(Type);
	ms.WriteArray(HardID);
	ms.Write(LastTime);
	ms.Write(Switchs);
	ms.Write(Analogs);
	ms.WriteString(Name);
}

void Device::Read(Stream& ms)
{
	ID		= ms.Read<byte>();
	Type	= ms.Read<ushort>();
	ms.ReadArray(HardID);
	LastTime= ms.Read<ulong>();
	Switchs	= ms.Read<byte>();
	Analogs	= ms.Read<byte>();
	ms.ReadString(Name);
}

bool operator==(const Device& d1, const Device& d2)
{
	return d1.ID == d2.ID;
}

bool operator!=(const Device& d1, const Device& d2)
{
	return d1.ID != d2.ID;
}
