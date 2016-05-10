#include "Flash.h"
#include "TinyServer.h"
#include "Security\Crc.h"

#include "JoinMessage.h"
#include "PingMessage.h"
#include "DataMessage.h"

#include "Config.h"
#include "Drivers\ShunCom.h"

#include "Security\MD5.h"

/******************************** TinyServer ********************************/

static bool OnServerReceived(void* sender, Message& msg, void* param);
static void GetDeviceKey(byte scr, Buffer& key,void* param);


TinyServer::TinyServer(TinyController* control)
{
	Control 	= control;
	Cfg			= nullptr;
	DeviceType	= Sys.Code;

	Control->Received	= OnServerReceived;
	Control->GetKey		= GetDeviceKey;
	Control->Param		= this;

	Control->Mode		= 2;	// 服务端接收所有消息

	Received	= nullptr;
	Param		= nullptr;

	Current		= nullptr;
	Study		= false;
}

bool TinyServer::Send(Message& msg) const
{
	// 附加目标物理地址
	//if(!msg.State)
	{
		auto dv	= DevMgmt.FindDev(((TinyMessage&)msg).Dest);
		if(!dv)	dv	= Current;
		//if(dv)	msg.State	= dv->Mac;
		if(dv)	dv->Mac.CopyTo(0, msg.State, -1);
	}

	return Control->Send(msg);
}

bool OnServerReceived(void* sender, Message& msg, void* param)
{
	auto server = (TinyServer*)param;
	assert_ptr(server);

	// 消息转发
	return server->OnReceive((TinyMessage&)msg);
}

// 常用系统级消息

void TinyServer::Start()
{
	TS("TinyServer::Start");

	assert(Cfg, "未指定微网服务器的配置");
	
	// 最后倒数8KB - 倒数位置4KB  的 4KB 空间
	//const uint DevAddr = 0x8000000 + (Sys.FlashSize << 10) - (8 << 10);
	//const uint DevSize = 4 << 10;
	//DevMgmt.SetFlashCfg(DevAddr,DevSize);

	auto count = DevMgmt.LoadDev();

	// 添加网关这一条设备信息
	if (!DevMgmt.FindDev(Cfg->Address))
	{
		// 如果全局设备列表内没有Server自己，则添加
		auto dv = new Device();
		dv->Address = Cfg->Address;
		dv->Kind = Sys.Code;
		dv->LastTime = Sys.Seconds();

		dv->HardID = Sys.ID;
		dv->Name = Sys.Name;

		// 放进持续在线表
		DevMgmt.OnlineAlways.Push(dv);
		//DevMgmt.PushDev(dv);
		//DevMgmt.SaveDev();
		DevMgmt.DeviceRequest(DeviceAtions::Register, dv);
	}
	// 注册Token控制设备列表时回调函数
	DevMgmt.Register([](DeviceAtions act, const Device* dv, void *param) {((TinyServer*)(param))->DevPrsCallback(act, dv); }, this);

	#if DEBUG
	Sys.AddTask([](void *param) {((DevicesManagement*)(param))->ShowDev(); }, &DevMgmt, 10000, 30000, "节点列表");
#endif

	Control->Open();
}

// 收到本地无线网消息
bool TinyServer::OnReceive(TinyMessage& msg)
{
	TS("TinyServer::OnReceive");

	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	auto dv = Current;
	if (!dv) dv = DevMgmt.FindDev(id);
	// 不响应不在设备列表设备的 非Join指令
	if(!dv && msg.Code > 2) return false;

	switch(msg.Code)
	{
		case 1:
			if(!OnJoin(msg)) return false;
			dv = Current;
			break;
		case 2:
			if(!OnDisjoin(msg))
				return false;
			break;
		case 3:
			// 设置当前设备
			Current = dv;
			OnPing(msg);
			break;
		case 5:
		case 0x15:
			if(msg.Reply)
			{
				// 修改最后读取时间
				dv->LastRead	= Sys.Seconds();
				OnReadReply(msg, *dv);
			}
			break;
		case 6:
		case 0x16:
			if(msg.Reply)
				OnWriteReply(msg, *dv);
			else
			{
				auto rs	= msg.CreateReply();
				if(OnWrite(msg, rs, *dv)) Send(rs);
			}
			break;
	}

	// 更新设备信息
	if(dv) dv->LastTime = Sys.Seconds();

	// 设置当前设备
	Current = dv;

	// 系统指令不会被转发，这里修改为用户指令
	if(msg.Code == 0x05 || msg.Code == 0x06) msg.Code |= 0x10;

	// 消息转发
	if(Received) return Received(this, msg, Param);

	Current = nullptr;

	return true;
}

// 分发外网过来的消息。返回值表示是否有响应
bool TinyServer::Dispatch(TinyMessage& msg)
{
	if(msg.Reply || msg.Error) return false;

	TS("TinyServer::Dispatch");

	// 先找到设备
	auto dv = DevMgmt.FindDev(msg.Dest);
	if(!dv) return false;

	// 设置当前设备
	Current	= dv;

	bool rt	= false;	// 是否响应远程
	bool fw	= true;		// 是否转发给本地

	auto rs	= msg.CreateReply();

	// 缓存内存操作指令
	switch(msg.Code)
	{
		case 5:
		case 0x15:
		{
			auto now	= Sys.Seconds();
			rt = OnRead(msg, rs, *dv);

			// 避免频繁读取。间隔秒数
			if(dv->LastRead + 5 < now)
			{
				//dv->LastRead	= now;
				rt	= false;
			}
			else
				fw	= false;

			break;
		}
		case 6:
		case 0x16:
			rt = false;
			break;
	}

	if(fw && !rs.Error)
	{
		// 非休眠设备直接发送
		//if(!dv->CanSleep())
		//{
			Send(msg);
		//}
		// 休眠设备进入发送队列
		//else
		//{

		//}
	}

	// 如果有返回，需要设置目标地址，让网关以为该信息来自设备
	if(rt)
	{
		msg.Dest	= rs.Dest;
		msg.Src		= dv->Address;
		msg.Reply	= true;
		msg.Error	= rs.Error;
		msg.SetData(Buffer(rs.Data, rs.Length));
	}

	Current = nullptr;

	return rt;
}

// 组网
bool TinyServer::OnJoin(const TinyMessage& msg)
{
	if(msg.Reply) return false;

	TS("TinyServer::OnJoin");

	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	if(!id) return false;

	auto now = Sys.Seconds();

	JoinMessage dm;
	dm.ReadMessage(msg);
	// 规避旧设备的错误数据
	if(dm.Kind == 0x1004) return false;

	// 根据硬件编码找设备
	auto dv = DevMgmt.FindDev(dm.HardID);
	if(!dv)
	{
		if(!Study)
		{
			debug_printf("非学习模式禁止加入\r\n");
			return false;
		}

		// 从1开始派ID
		id	= 1;
		while(DevMgmt.FindDev(++id) != nullptr && id < 0xFF);
		debug_printf("发现节点设备 0x%04X ，为其分配 0x%02X\r\n", dm.Kind, id);
		if(id == 0xFF) return false;

		dv = new Device();
		dv->Address	= id;
		dv->Logins	= 0;
		// 节点注册
		dv->RegTime	= now;
		dv->Kind	= dm.Kind;
		//dv->SetHardID(dm.HardID);
		//dv->HardID	= dm.HardID;
		dv->HardID.Copy(0, dm.HardID, 0, -1);
		dv->Version	= dm.Version;
		dv->LoginTime = now;
		// 生成随机密码。当前时间的MD5
		//auto bs	= MD5::Hash(Buffer(&now, 8));
		//if(bs.Length() > 8) bs.SetLength(8);
		//dv->SetPass(bs);
		dv->Pass	= MD5::Hash(Buffer(&now, 8)).Sub(0, 8);

		// 保存无线物理地址
		auto st = (byte*)msg.State;
		if(st)
		{
			//byte sum = st[0] && st[1] && st[2] && st[3] && st[4];
			int sum = (int)st[0] + st[1] + st[2] + st[3] + st[4];
			if(sum == 0 || sum == 0xFF * 5) st = nullptr;
		}
		if(!st)
			dv->Mac.Copy(0,dv->HardID,0, dv->Mac.Length());
		else
			dv->Mac	= st;

		if(dv->Valid())
		{
			DevMgmt.DeviceRequest(DeviceAtions::Register, dv);
			//DevMgmt.PushDev(dv);
			//DevMgmt.SaveDev();	// 写好相关数据 校验通过才能存flash
		}
		else
		{
			delete dv;
			return false;
		}
	}

	// 更新设备信息
	Current		= dv;
	dv->LoginTime	= now;
	dv->Logins++;

	debug_printf("\r\nTinyServer::设备第 %d 次组网 TranID=0x%04X \r\n", dv->Logins, dm.TranID);
	dv->Show(true);

	// 响应
	auto rs	= msg.CreateReply();

	// 发现响应
	dm.Reply	= true;
	dm.Server	= Cfg->Address;
	dm.Channel	= Cfg->Channel;
	dm.Speed	= Cfg->Speed / 10;
	dm.Address	= dv->Address;
	//dm.Password.Copy(dv->GetPass());
	dm.Password	= dv->Pass;

	//dm.HardID.Set(Sys.ID, 6);
	//dm.HardID.Copy(0, Sys.ID, 6);
	dm.HardID.SetLength(6);
	dm.HardID	= Sys.ID;
	dm.WriteMessage(rs);
	//dm.Show(true);
	//rs.Show();

	rs.State	= dv->_Mac;
	//Control->Send(rs);
	// 组网消息属于广播消息，很可能丢包，重发3次
	for(int i=0; i<3; i++)
	{
		Control->Send(rs);
		Sys.Sleep(10);
	}

	return true;
}

// 读取
bool TinyServer::OnDisjoin(const TinyMessage& msg)
{
	TS("TinyServer::OnDisjoin");

	debug_printf("TinyServer::OnDisjoin\r\n");
	// 如果是退网请求，这里也需要删除设备
	if(!msg.Reply)
	{
		auto dv	= DevMgmt.FindDev(msg.Src);
		if(dv)
		{
			// 拿出来硬件ID的校验，检查是否合法
			auto ms 	= msg.ToStream();
			ushort crc1	= ms.ReadUInt16();
			ushort crc2	= Crc::Hash16(dv->HardID);
			if(crc1 == crc2)
			{
				debug_printf("TinyServer::OnDisjoin:0x%02X \r\n", dv->Address);
				//DeleteDevice(dv->Address);
				return true;
			}
			else
			{
				debug_printf("0x%02X 非法退网，请求的硬件校验 0x%04X 不等于本地硬件校验 0x%04X", dv->Address, crc1, crc2);
				return false;
			}
		}
	}

	return false;
}

bool TinyServer::Disjoin(TinyMessage& msg, ushort crc) const
{
	TS("TinyServer::Disjoin");

	msg.Code = 0x02;
	auto ms = msg.ToStream();
	ms.Write(crc);
	msg.Length = ms.Position();

	Send(msg);

	return true;
}

bool TinyServer::Disjoin(byte id)
{
	TS("TinyServer::Disjoin");

	auto dv  = DevMgmt.FindDev(id);
	auto crc = Crc::Hash16(dv->HardID);

	TinyMessage msg;

	msg.Code = 0x02;
	auto ms = msg.ToStream();
	ms.Write(crc);
	msg.Length = ms.Position();

	Send(msg);
	DevMgmt.DeviceRequest(DeviceAtions::Delete,id);

	return true;
}
// 心跳保持与对方的活动状态
bool TinyServer::OnPing(const TinyMessage& msg)
{
	TS("TinyServer::OnPing");

	auto dv = DevMgmt.FindDev(msg.Src);
	// 网关内没有相关节点信息时不鸟他
	if(dv == nullptr) return false;

	auto rs	= msg.CreateReply();
	auto ms	= msg.ToStream();

	PingMessage pm;
	pm.MaxSize	= Control->Port->MaxSize - TinyMessage::MinSize;
	// 子操作码
	while(ms.Remain())
	{
		byte code	= ms.ReadByte();
		switch(code)
		{
			case 0x01:
			{
				//auto bs = dv->GetStore();
				pm.ReadData(ms, dv->Store);

				// 更新读取时间
				dv->LastRead	= Sys.Seconds();

				break;
			}
			case 0x02:
			{
				//auto bs = dv->GetConfig();
				//pm.ReadData(ms, bs);
				break;
			}
			case 0x03:
			{
				ushort crc	= 0;
				if(!pm.ReadHardCrc(ms, *dv, crc))
				{
					Disjoin(rs, crc);
					return false;
				}
				break;
			}
			default:
			{
				debug_printf("TinyServer::OnPing 无法识别的心跳子操作码 0x%02X \r\n", code);
				return false;
			}
		}
	}
	// 告诉客户端有多少待处理指令

	// 给客户端同步时间，4字节的秒
	auto ms2	= rs.ToStream();
	pm.WriteTime(ms2, Sys.Seconds());
	rs.Length	= ms2.Position();

	Send(rs);

	return true;
}

/*
请求：1起始 + 1大小
响应：1起始 + N数据
错误：错误码2 + 1起始 + 1大小
*/
bool TinyServer::OnRead(const Message& msg, Message& rs, const Device& dv)
{
	if(msg.Reply) return false;
	if(msg.Length < 2) return false;
	if(msg.Error) return false;

	TS("TinyServer::OnRead");

	auto ms	= rs.ToStream();

	DataMessage dm(msg, &ms);

	bool rt	= true;
	if(dm.Offset < 64)
		rt	= dm.ReadData(dv.Store);
	else if(dm.Offset < 128)
	{
		dm.Offset	-= 64;
		//rt	= dm.ReadData(dv.GetConfig());
	}

	rs.Error	= !rt;
	rs.Length	= ms.Position();
	//rs.Show();

	return true;
}

/*
请求：1起始 + N数据
响应：1起始 + 1大小
错误：错误码2 + 1起始 + 1大小
*/
bool TinyServer::OnWrite(const Message& msg, Message& rs, Device& dv)
{
	if(msg.Reply) return false;
	if(msg.Length < 2) return false;

	TS("TinyServer::OnWrite");

	auto ms	= rs.ToStream();

	DataMessage dm(msg, &ms);

	bool rt	= true;
	if(dm.Offset < 64)
		rt	= dm.WriteData(dv.Store, false);
	else if(dm.Offset < 128)
	{
		dm.Offset	-= 64;
		//rt	= dm.WriteData(dv.GetConfig(), false);
	}

	rs.Error	= !rt;
	rs.Length	= ms.Position();

	return true;
}

// 读取响应，服务端趁机缓存一份。
bool TinyServer::OnReadReply(const Message& msg, Device& dv)
{
	if(!msg.Reply || msg.Error) return false;
	if(msg.Length < 2) return false;

	TS("TinyServer::OnReadReply");

	return OnWriteReply(msg, dv);
}

// 节点的写入响应，偏移和长度之后可能携带有数据
bool TinyServer::OnWriteReply(const Message& msg, Device& dv)
{
	if(!msg.Reply || msg.Error) return false;
	if(msg.Length <= 2) return false;

	TS("TinyServer::OnWriteReply");

	DataMessage dm(msg, nullptr);

	if(dm.Offset < 64)
		dm.WriteData(dv.Store, false);
	else if(dm.Offset < 128)
	{
		dm.Offset	-= 64;
		//dm.WriteData(dv.GetConfig(), false);
	}

	return true;
}

// 设置zigbee的通道，2401无效
void TinyServer::SetChannel(byte channel)
{
	if(!Control) return;

	auto zb = (ShunCom*)Control;
	if(!zb) return;

	if(zb->EnterConfig())
	{
	  zb->ShowConfig();
	  zb->SetChannel(channel);
	  zb->ExitConfig();
	}
}

void GetDeviceKey(byte scr, Buffer& key, void* param)
{
	/*TS("TinyServer::GetDeviceKey");

	auto server = (TinyServer*)param;
	auto devMgmt = &(server->DevMgmt);

	auto dv = devMgmt->FindDev(scr);
	if(!dv) return;

	// 检查版本
	if(dv->Version < 0x00AA) return;

	// debug_printf("%d 设备获取密匙\n",scr);
	key.Copy(dv->Pass, 8);*/
}

void TinyServer::ClearDevices()
{
	TS("TinyServer::ClearDevices");
	debug_printf("准备清空设备，发送 Disjoin");

	int count = DevMgmt.Length();
	for(int j = 0; j < 3; j++)		// 3遍
	{
		for(int i = 1; i < count; i++)	// 从1开始派ID  自己下线完全不需要
		{
			auto dv = DevMgmt.DevArr[i];
			if(!dv)continue;

			TinyMessage rs;
			rs.Dest = dv->Address;
			ushort crc = Crc::Hash16(dv->HardID);
			Disjoin(rs, crc);
			debug_printf(".");
		}
	}
	debug_printf("\r\n发送 Disjoin 完毕\r\n");
	DevMgmt.ClearDev();
}

void TinyServer::DevPrsCallback(DeviceAtions act, const Device * dv)
{
	TS("TinyServer::DevPrsCallback");
	if (!dv)return;
	switch (act)
	{
		case DeviceAtions::Delete:
			{
				auto crc = Crc::Hash16(dv->HardID);

				TinyMessage msg;

				msg.Code = 0x02;
				auto ms = msg.ToStream();
				ms.Write(crc);
				msg.Length = ms.Position();
				Send(msg);
			}
			break;/*
		case DeviceAtions::List:
			break;
		case DeviceAtions::Update:
			break;
		case DeviceAtions::Register:
			break;
		case DeviceAtions::Online:
			break;
		case DeviceAtions::Offline:
			break;
		case DeviceAtions::ListIDs:
			break;*/
		default:
			break;
	}

}
