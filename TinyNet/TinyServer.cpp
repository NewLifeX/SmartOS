#include "Time.h"
#include "TinyServer.h"

#include "JoinMessage.h"

#include "Config.h"

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

// 收到本地无线网消息
bool TinyServer::OnReceive(TinyMessage& msg)
{
	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	Device* dv = Current;
	if(!dv) dv = FindDevice(id);

	switch(msg.Code)
	{
		case 1:
			OnJoin(msg);
			dv = Current;
			break;
		case 2:
			OnDisjoin(msg);
			break;
		case 3:
			OnPing(msg);
			break;
		case 5:
			// 系统指令不会被转发，这里修改为用户指令
			msg.Code = 0x15;
		case 0x15:
			OnReadReply(msg, *dv);
			break;
		case 6:
		case 0x16:
			//OnWriteReply(msg, *dv);
			break;
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

// 分发外网过来的消息。返回值表示是否有响应
bool TinyServer::Dispatch(TinyMessage& msg)
{
	// 先找到设备
	Device* dv = FindDevice(msg.Dest);
	if(!dv) return false;

	bool rs = false;
	
	// 缓存内存操作指令
	switch(msg.Code)
	{
		case 5:
		case 0x15:
			rs = OnRead(msg, *dv);
			break;
		case 6:
		case 0x16:
			rs = OnWrite(msg, *dv);
			break;
	}

	// 非休眠设备直接发送
	if(!dv->CanSleep())
	{
		Send(msg);
	}
	// 休眠设备进入发送队列
	else
	{
		
	}
	
	return rs;
}

// 组网
bool TinyServer::OnJoin(TinyMessage& msg)
{
	if(msg.Reply) return false;

	JoinMessage dm;
	dm.ReadMessage(msg);

	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	if(!id) return false;

	Device* dv = FindDevice(dm.HardID);
	if(!dv)
	{
		// 以网关地址为基准，进行递增分配
		byte addr = Config.Address;
		// 查找该ID是否存在，如果不同设备有相同ID，则从0x02开始主动分配
		if(FindDevice(id) != NULL)
		{
			id = addr;
			while(FindDevice(++id) != NULL && id < 0xFF);

			debug_printf("发现ID=0x%02X已分配，为当前节点分配 0x%02X\r\n", msg.Src, id);
		}
		else
		{
			id = Devices.Count() + addr;
			// 注意，网关可能来不及添加
			if(id <= addr) id = addr + 1;
			debug_printf("发现节点设备 0x%02X ，为其分配 0x%02X\r\n", msg.Src, id);
		}

		dv = new Device();
		dv->Address		= id;

		Devices.Add(dv);

		// 节点注册
		dv->RegTime	= Time.Current();
	}
	// 更新设备信息
	if(dv)
	{
		Current		= dv;

		dv->Type	= dm.Kind;
		dv->HardID	= dm.HardID;
		dv->Version	= dm.Version;
		//dv->Switchs	= dm.Switchs;
		//dv->Analogs	= dm.Analogs;

		// 如果最后活跃时间超过60秒，则认为是设备上线
		if(dv->LastTime == 5 || dv->LastTime + 6000000 < Time.Current())
		{
			// 节点上线
			dv->LoginTime = dv->RegTime;
		}
		dv->LastTime = Time.Current();

		debug_printf("\r\nTinyServer::新设备组网 0x%08X \r\n", dm.TranID);
		dv->Show(true);

		// 对于已注册的设备，再来发现消息不做处理
		//if(isNew)
		{
			// 生成随机密码。当前时间的MD5
			ulong now = Time.Current();
			ByteArray bs((byte*)&now, 8);
			dv->Pass = MD5::Hash(bs);
			dv->Pass.SetLength(8);	// 小心不要超长

			// 响应
			TinyMessage rs;
			rs.Code = msg.Code;
			rs.Dest = msg.Src;
			rs.Sequence	= msg.Sequence;

			// 发现响应
			//JoinMessage dm;
			dm.Reply	= true;

			dm.Server	= Config.Address;
			dm.Channel	= Config.Channel;
			dm.Speed	= Config.Speed == 250 ? 0 : (Config.Speed == 1000 ? 1 : 2);;

			dm.Address	= dv->Address;
			dm.Password	= dv->Pass;

			dm.HardID.SetLength(6);	// 小心不要超长
			dm.HardID	= Sys.ID;

			dm.WriteMessage(rs);

			Reply(rs);
		}
	}

	return true;
}

// 读取
bool TinyServer::OnDisjoin(TinyMessage& msg)
{
	return true;
}

// 心跳保持与对方的活动状态
bool TinyServer::OnPing(TinyMessage& msg)
{
	// 网关内没有相关节点信息时不鸟他
	if(FindDevice(msg.Src) == NULL)return false;
	if(!msg.Reply) Reply(msg);

	return true;
}

// 读取
bool TinyServer::OnRead(TinyMessage& msg, Device& dv)
{
	if(msg.Reply) return false;
	if(msg.Length < 2) return false;

	// 起始地址为7位压缩编码整数
	Stream ms = msg.ToStream();
	uint offset = ms.ReadEncodeInt();
	uint len	= ms.ReadEncodeInt();

	ByteArray& bs = dv.Store;
	
	// 重新一个数据流，避免前面的不够
	Stream ms2(4 + len);
	ms2.WriteEncodeInt(offset);

	int remain = bs.Length() - offset;
	if(remain < 0)
	{
		// 出错，使用原来的数据区即可，只需要返回一个起始位置
		msg.Error = true;
	}
	else
	{
		if(len > remain) len = remain;
		if(len > 0) ms2.Write(bs.GetBuffer(), offset, len);
	}
	msg.SetData(ms2.GetBuffer(), ms2.Position());
	msg.Reply = true;

	return true;
}

// 读取响应，服务端趁机缓存一份。定时上报也是采用该指令。
bool TinyServer::OnReadReply(TinyMessage& msg, Device& dv)
{
	if(!msg.Reply || msg.Error) return false;
	if(msg.Length < 2) return false;

	// 起始地址为7位压缩编码整数
	Stream ms = msg.ToStream();
	uint offset = ms.ReadEncodeInt();

	ByteArray& bs = dv.Store;
	
	int remain = bs.Length() - offset;
	if(remain < 0) return false;

	uint len = ms.Remain();
	if(len > remain) len = remain;
	// 保存一份到缓冲区
	if(len > 0)
	{
		bs.Copy(ms.Current(), len, offset);
	}

	return true;
}

// 写入
bool TinyServer::OnWrite(TinyMessage& msg, Device& dv)
{
	if(msg.Reply) return false;
	if(msg.Length < 2) return false;

	// 起始地址为7位压缩编码整数
	Stream ms = msg.ToStream();
	uint offset = ms.ReadEncodeInt();

	ByteArray& bs = dv.Store;
	
	int remain = bs.Length() - offset;
	if(remain < 0)
	{
		// 出错，使用原来的数据区即可，只需要返回一个起始位置
		msg.Error = true;
		ms.Write(1);
		ms.Write(0);
	}
	else
	{
		uint len = ms.Remain();
		if(len > remain) len = remain;
		// 保存一份到缓冲区
		if(len > 0)
		{
			bs.Copy(ms.Current(), len, offset);
			// 实际写入的长度
			ms.WriteEncodeInt(len);
		}
	}
	msg.Length = ms.Position();
	msg.Reply = true;

	return true;
}

Device* TinyServer::FindDevice(byte id)
{
	if(id == 0) return NULL;

	for(int i=0; i<Devices.Count(); i++)
	{
		if(id == Devices[i]->Address) return Devices[i];
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
	if(dv && dv->Address == id)
	{
		Devices.Remove(dv);
		delete dv;
		return true;
	}

	return false;
}
