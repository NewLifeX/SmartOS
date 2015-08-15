#include "Time.h"
#include "TinyServer.h"

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

	Current		= NULL;
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
	Device* dv = Current;
	if(!dv) dv = FindDevice(id);
	if(!dv && id)
	{
		dv = new Device();
		dv->ID = id;
		// 默认作为开关
		dv->Type = 0x0203;
		dv->Switchs = 3;

		Devices.Add(dv);

		dv->RegTime	= Time.Current();
		dv->LoginTime = dv->RegTime;
	}
	// 更新设备信息
	if(dv) dv->LastTime = Time.Current();

	// 设置当前设备
	Current = dv;

	// 消息转发
	if(Received) return Received(msg, Param);

	Current = NULL;

	return true;
}

// 设备发现
// 请求：2字节设备类型 + 20字节系统ID
// 响应：1字节地址 + 8字节密码
bool TinyServer::OnDiscover(TinyMessage& msg)
{
	if(msg.Reply) return false;

	DiscoverMessage dm;
	dm.ReadMessage(msg);

	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	if(!id) return false;

	Device* dv = FindDevice(dm.HardID);
	//Device* dv = FindDevice(id);
	//bool isNew = false;
	if(!dv)
	{
		//isNew = true;

		// 查找该ID是否存在，如果不同设备有相同ID，则从0x02开始主动分配
		if(FindDevice(id) != NULL)
		{
			id = 1;
			while(FindDevice(++id) != NULL && id < 0xFF);

			debug_printf("发现ID=0x%02X已分配，为当前节点分配 0x%02X\r\n", msg.Src, id);
		}
		else
		{
			id = Devices.Count() + 1;
			// 注意，网关可能来不及添加
			if(id <= 1) id = 2;
			debug_printf("发现节点设备 0x%02X ，为其分配 0x%02X\r\n", msg.Src, id);
		}

		dv = new Device();
		dv->ID		= id;

		Devices.Add(dv);

		// 节点注册
		dv->RegTime	= Time.Current();
	}
	// 更新设备信息
	if(dv)
	{
		Current		= dv;

		dv->Type	= dm.Type;
		dv->HardID	= dm.HardID;
		dv->Version	= dm.Version;
		dv->Switchs	= dm.Switchs;
		dv->Analogs	= dm.Analogs;

		// 如果最后活跃时间超过60秒，则认为是设备上线
		if(dv->LastTime == 5 || dv->LastTime + 6000000 < Time.Current())
		{
			// 节点上线
			dv->LoginTime = dv->RegTime;
		}
		dv->LastTime = Time.Current();

		debug_printf("\r\nTinyServer::Discover ");
		dv->Show(true);

		// 对于已注册的设备，再来发现消息不做处理
		//if(isNew)
		{
			// 生成随机密码。当前时间的MD5
			ulong now = Time.Current();
			ByteArray bs((byte*)&now, 8);
			MD5::Hash(bs, dv->Pass);

			// 响应
			TinyMessage rs;
			rs.Code = msg.Code;
			rs.Dest = msg.Src;
			rs.Sequence	= msg.Sequence;

			// 发现响应
			DiscoverMessage dm;
			dm.Reply	= true;
			dm.ID		= dv->ID;
			dm.Pass		= dv->Pass;
			dm.WriteMessage(rs);

			Reply(rs);
		}
	}

	return true;
}

// 心跳保持与对方的活动状态
bool TinyServer::OnPing(TinyMessage& msg)
{
	if(!msg.Reply) Reply(msg);

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

Device* TinyServer::FindDevice(ByteArray& hardid)
{
	if(hardid.Length() == 0) return NULL;

	for(int i=0; i<Devices.Count(); i++)
	{
		if(hardid == Devices[i]->HardID) return Devices[i];
	}

	return NULL;
}

bool TinyServer::DeleteDevice(byte id)
{
	Device* dv = FindDevice(id);
	if(dv && dv->ID == id)
	{
		Devices.Remove(dv);
		delete dv;
		return true;
	}
	
	return false;
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
	str = str + "ID=0x" + ID;
	str = str + " Type=" + (byte)(Type >> 8) + (byte)(Type & 0xFF);
	str = str + " Name=" + Name;
	str = str + " HardID=" + HardID;

	DateTime dt;
	dt.Parse(LastTime);
	str = str + " LastTime=" + dt.ToString();

	str = str + " Switchs=" + Switchs;
	str = str + " Analogs=" + Analogs;

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
