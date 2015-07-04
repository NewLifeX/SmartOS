#include "TinyServer.h"
#include "SerialPort.h"

#include "TinyMessage.h"
#include "TokenMessage.h"

#include "TokenNet\DiscoverMessage.h"

#include "Security\MD5.h"

/******************************** TinyServer ********************************/

bool OnServerReceived(Message& msg, void* param);

TinyServer::TinyServer(TinyController* control)
{
	Control 	= control;

	DeviceType	= Sys.Code;

	Control->Received	= OnServerReceived;
	Control->Param		= this;

	Received	= NULL;
	Param		= NULL;
}

bool TinyServer::Send(Message& msg)
{
	return Control->Send(msg);
}

bool TinyServer::Reply(Message& msg)
{
	return Control->Reply(msg);
}

bool OnServerReceived(Message& msg, void* param)
{
	TinyServer* server = (TinyServer*)param;
	assert_ptr(server);

	// 消息转发
	return server->OnReceive((TinyMessage&)msg);
}

// 常用系统级消息

void TinyServer::Start()
{
	Control->Open();
}

bool TinyServer::OnReceive(TinyMessage& msg)
{
	switch(msg.Code)
	{
		case 1:
			OnDiscover(msg);
			break;
		case 2:
			OnPing(msg);
			break;
		case 3:
			OnSysTime(msg);
			break;
		case 4:
			OnSysID(msg);
			break;
		case 5:
			OnSysMode(msg);
			break;
	}

	// 临时方便调试使用
	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	Device* dv = FindDevice(id);
	if(!dv && id)
	{
		dv = new Device();
		dv->ID = id;
		// 默认作为开关
		dv->Type = 0x0203;

		Devices.Add(dv);

		dv->RegTime	= Time.Current();
		dv->LoginTime = dv->RegTime;
	}
	// 更新设备信息
	if(dv) dv->LastTime = Time.Current();

	// 消息转发
	if(Received) return Received(msg, Param);

	return true;
}

// 设备发现
// 请求：2字节设备类型 + 20字节系统ID
// 响应：1字节地址 + 8字节密码
bool TinyServer::OnDiscover(TinyMessage& msg)
{
	if(msg.Reply) return false;

	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	Device* dv = FindDevice(id);
	bool isNew = false;
	if(!dv && id)
	{
		isNew = true;

		DiscoverMessage dm;
		dm.ReadMessage(msg);

		dv = new Device();
		dv->ID		= id;
		dv->Type	= dm.Type;
		dv->HardID	= dm.HardID;

		Devices.Add(dv);

		debug_printf("TinyServer::Discover ");
		dv->Show(true);

		// 节点注册
		dv->RegTime	= Time.Current();
	}
	// 更新设备信息
	if(dv)
	{
		// 如果最后活跃时间超过60秒，则认为是设备上线
		if(dv->LastTime == 5 || dv->LastTime + 60000000 < Time.Current())
		{
			// 节点上线
			dv->LoginTime = dv->RegTime;
		}
		dv->LastTime = Time.Current();

		// 对于已注册的设备，再来发现消息不做处理
		if(isNew)
		{
			// 生成随机密码。当前时间的MD5
			ulong now = Time.Current();
			ByteArray bs((byte*)&now, 8);
			MD5::Hash(bs, dv->Pass);

			// 响应
			Stream ms(msg.Data, msg.Length);
			ms.Write(id);
			ms.WriteArray(dv->Pass);

			Reply(msg);
		}
	}

	return true;
}

// 心跳保持与对方的活动状态
bool TinyServer::OnPing(TinyMessage& msg)
{
	return true;
}

// 询问及设置系统时间
bool TinyServer::OnSysTime(TinyMessage& msg)
{
	return true;
}

// 询问系统标识号
bool TinyServer::OnSysID(TinyMessage& msg)
{
	return true;
}

// 设置系统模式
bool TinyServer::OnSysMode(TinyMessage& msg)
{
	return true;
}

Device* TinyServer::FindDevice(byte id)
{
	if(id == 0) return NULL;

	for(int i=0; i<Devices.Count(); i++)
	{
		if(id == Devices[i]->ID) return Devices[i];
	}

	return NULL;
}

/******************************** Device ********************************/

Device::Device() : HardID(0), Name(0), Pass(0)
{
	ID			= 0;
	Type		= 0;
	LastTime	= 0;
	Switchs		= 0;
	Analogs		= 0;

	RegTime		= 0;
	LoginTime	= 0;
}

void Device::Write(Stream& ms) const
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

String& Device::ToStr(String& str) const
{
	str.Format("ID=0x%02X Type=0x%04X", ID, Type);
	str.Format(" Name=");
	str += Name;
	str.Format(" HardID=");
	str += HardID;
	str.Format(" LastTime=");
	DateTime dt;
	dt.Parse(LastTime);
	str += dt.ToString();
	str.Format(" Switchs=%d Analogs=%d", Switchs, Analogs);

	return str;
}

bool operator==(const Device& d1, const Device& d2)
{
	return d1.ID == d2.ID;
}

bool operator!=(const Device& d1, const Device& d2)
{
	return d1.ID != d2.ID;
}
