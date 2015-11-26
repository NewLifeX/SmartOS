#include "Flash.h"
#include "TinyServer.h"
#include "Security\Crc.h"

#include "JoinMessage.h"

#include "Config.h"

#include "Security\MD5.h"

/******************************** TinyServer ********************************/

static bool OnServerReceived(Message& msg, void* param);

TinyServer::TinyServer(TinyController* control)
{
	Control 	= control;
	Cfg			= NULL;
	DeviceType	= Sys.Code;

	Control->Received	= OnServerReceived;
	Control->Param		= this;

	Control->Mode		= 2;	// 服务端接收所有消息
	DataStoreLent		= 64;

	Received	= NULL;
	Param		= NULL;

	Current		= NULL;
	Study		= false;

	Devices.SetLength(0);
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
	assert_param2(Cfg, "未指定微网服务器的配置");

	//LoadConfig();
	LoadDevices();

	Control->Open();
}

// 收到本地无线网消息
bool TinyServer::OnReceive(TinyMessage& msg)
{
	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	Device* dv = Current;
	if(!dv) dv = FindDevice(id);
	// 不响应不在设备列表设备的 非Join指令
	if((!dv) && (msg.Code > 2))return false;
	
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
			// 设置当前设备
			Current = dv;
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
	if(dv) dv->LastTime = Sys.Ms();

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
	// 非休眠设备直接发送
	//if(!dv->CanSleep())
	//{
		Send(msg);
	//}
	// 休眠设备进入发送队列
	//else
	//{

	//}

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

	// 如果有返回，需要设置目标地址，让网关以为该信息来自设备
	if(rs)
	{
		msg.Dest	= msg.Src;
		msg.Src		= dv->Address;
	}

	return rs;
}

// 组网
bool TinyServer::OnJoin(const TinyMessage& msg)
{
	if(msg.Reply)
	{
		return false;
	}
	  if(!Study)
	  {
		  debug_printf("非学习模式禁止加入\r\n");
		  return false;
	  }
	// 如果设备列表没有这个设备，那么加进去
	byte id = msg.Src;
	if(!id) return false;

	ulong now = Sys.Ms();

	JoinMessage dm;
	dm.ReadMessage(msg);
	// 根据硬件编码找设备
	Device* dv = FindDevice(dm.HardID);
	if(!dv)
	{
		// 以网关地址为基准，进行递增分配
		byte addr = Cfg->Address;
		{
			id = addr;
			while(FindDevice(++id) != NULL && id < 0xFF);
			debug_printf("发现节点设备 0x%04X ，为其分配 0x%02X\r\n", dm.Kind, id);
			if(id == 0xFF) return false;
		}
		dv = new Device();
		dv->Address	= id;
		dv->Logins= dv->Store.Length();

		// 节点注册
		dv->RegTime	= now;

		Devices.Push(dv);

		SaveDevices();
	}

	// 更新设备信息
	Current		= dv;

	dv->Kind	= dm.Kind;
	dv->HardID	= dm.HardID;
	dv->Version	= dm.Version;

	if(dv->Logins++ == 0) dv->LoginTime = now;
	dv->LastTime = now;

	debug_printf("\r\nTinyServer::新设备第 %d 次组网 TranID=0x%08X \r\n", dv->Logins, dm.TranID);
	dv->Show(true);

	// 生成随机密码。当前时间的MD5
	dv->Pass = MD5::Hash(Array(&now, 8));
	dv->Pass.SetLength(8);	// 小心不要超长

	// 响应
	TinyMessage rs;
	rs.Code = msg.Code;
	rs.Dest = msg.Src;
	rs.Sequence	= msg.Sequence;

	// 发现响应
	dm.Reply	= true;

	dm.Server	= Cfg->Address;
	dm.Channel	= Cfg->Channel;
	dm.Speed	= Cfg->Speed / 10;

	dm.Address	= dv->Address;
	dm.Password	= dv->Pass;

	dm.HardID.SetLength(6);	// 小心不要超长
	dm.HardID	= Sys.ID;

	dm.WriteMessage(rs);

	Reply(rs);

	return true;
}

//网关重置节点通信密码
bool TinyServer::ResetPassword(byte id)
{

	ulong now = Sys.Ms();

	JoinMessage dm;

	// 根据硬件编码找设备
	Device* dv = FindDevice(id);

	if(!dv) return false;

	// 更新设备信息
	//Current		= dv;

	//if(dv->Logins++ == 0) dv->LoginTime = now;
	//dv->LastTime = now;


	// 生成随机密码。当前时间的MD5
	dv->Pass = MD5::Hash(Array(&now, 8));
	dv->Pass.SetLength(8);	// 小心不要超长

	// 响应
	TinyMessage rs;
	rs.Code = 0x01;
	rs.Dest = id;
	rs.Sequence	= id;

	// 发现响应
	dm.Reply	= true;

	dm.Server	= Cfg->Address;
	dm.Channel	= Cfg->Channel;
	dm.Speed	= Cfg->Speed / 10;

	dm.Address	= dv->Address;
	dm.Password	= dv->Pass;

	dm.HardID.SetLength(6);	// 小心不要超长
	dm.HardID	= Sys.ID;

	dm.WriteMessage(rs);

	Reply(rs);

	return true;

}

// 读取
bool TinyServer::OnDisjoin(const TinyMessage& msg)
{
	return true;
}

bool TinyServer::Disjoin(TinyMessage& msg,uint crc)
{
	msg.Code = 0x02;
	Stream ms = msg.ToStream();
	ms.Write(crc);
	msg.Length = ms.Position();

	Send(msg);

	return true;
}
// 心跳保持与对方的活动状态
bool TinyServer::OnPing(const TinyMessage& msg)
{
	Device* dv = FindDevice(msg.Src);
	// 网关内没有相关节点信息时不鸟他
	if(dv == NULL)return false;

	// 准备一条响应指令
	TinyMessage rs;
	rs.Code = msg.Code;
	rs.Dest = msg.Src;
	rs.Sequence	= msg.Sequence;

	Stream ms	= msg.ToStream();
	// 子操作码
	while(ms.Remain())
	{
		switch(ms.ReadByte())
		{
			// 同步数据
			case 0x01:
			{
				byte offset	= ms.ReadByte();
				byte len	= ms.ReadByte();
				debug_printf("设备 0x%02X 同步数据（%d, %d）到网关缓存 \r\n", dv->Address, offset, len);

				int remain	= dv->Store.Capacity() - offset;
				int len2	= len;
				if(len2 > remain) len2 = remain;
				// 保存一份到缓冲区
				if(len2 > 0)
				{
					dv->Store.Copy(ms.ReadBytes(len), len2, offset);
				}
				break;
			}
			case 0x02:
			{
				break;
			}
			case 0x03:
			{
				ushort crc  = ms.ReadUInt16();
				ushort crc1 = Crc::Hash16(dv->HardID);
				// 下一行仅调试使用
				//debug_printf("设备硬件Crc: %08X, 本地Crc：%08X \r\n", crc, crc1);
				if(crc != crc1)
				{
					debug_printf("设备硬件Crc: %04X, 本地Crc：%04X \r\n", crc, crc1);
					debug_printf("设备硬件ID: ");
					dv->HardID.Show(true);

					Disjoin(rs, crc);

					return false;
				}
				break;
			}
			default:break;
		}
	}
	//todo。告诉客户端有多少待处理指令

	Reply(rs);

	return true;
}

/*
请求：1起始 + 1大小
响应：1起始 + N数据
错误：错误码2 + 1起始 + 1大小
*/
bool TinyServer::OnRead(TinyMessage& msg, Device& dv)
{
	if(msg.Reply) return false;
	if(msg.Length < 2) return false;
	if(msg.Error) return false;

	// 起始地址为7位压缩编码整数
	Stream ms	= msg.ToStream();
	uint offset = ms.ReadEncodeInt();
	uint len	= ms.ReadEncodeInt();

	// 指针归零，准备写入响应数据
	ms.SetPosition(0);

	// 计算还有多少数据可读
	int remain = dv.Store.Length() - offset;

	  while(remain<0)
	  {
		  debug_printf("读取数据出错Store.Length=%d \r\n",dv.Store.Length()) ;
		  offset--;

		  remain = dv.Store.Length() - offset;
	  }

	if(remain < 0)
	{
		// 可读数据不够时出错
		msg.Error = true;
		ms.Write((byte)2);
		ms.WriteEncodeInt(offset);
		ms.WriteEncodeInt(len);
	}
	else
	{
		ms.WriteEncodeInt(offset);
		// 限制可以读取的大小，不允许越界
		if(len > remain) len = remain;
		if(len > 0) ms.Write(dv.Store.GetBuffer(), offset, len);
	}
	msg.Length	= ms.Position();
	msg.Reply	= true;

	return true;
}

// 读取响应，服务端趁机缓存一份。定时上报也是采用该指令。
bool TinyServer::OnReadReply(const TinyMessage& msg, Device& dv)
{
	if(!msg.Reply || msg.Error) return false;
	if(msg.Length < 2) return false;

	 debug_printf("响应读取写入数据 \r\n") ;
	// 起始地址为7位压缩编码整数
	Stream ms	= msg.ToStream();
	uint offset = ms.ReadEncodeInt();

	int remain = dv.Store.Capacity() - offset;

	if(remain < 0) return false;

	uint len = ms.Remain();
	if(len > remain) len = remain;
	// 保存一份到缓冲区
	if(len > 0)
	{
		dv.Store.Copy(ms.Current(), len, offset);
	}

	return true;
}

/*
请求：1起始 + N数据
响应：1起始 + 1大小
错误：错误码2 + 1起始 + 1大小
*/
bool TinyServer::OnWrite(TinyMessage& msg, Device& dv)
{
	if(msg.Reply) return false;
	if(msg.Length < 2) return false;

	// 起始地址为7位压缩编码整数
	Stream ms	= msg.ToStream();
	uint offset = ms.ReadEncodeInt();

	if(offset>DataStoreLent)
	{
		return true;
	}
	// 计算还有多少数据可写
	uint len = ms.Remain();
	int remain = dv.Store.Capacity() - offset;
	if(remain < 0)
	{
		msg.Error = true;

		// 指针归零，准备写入响应数据
		ms.SetPosition(0);

		ms.Write((byte)2);
		ms.WriteEncodeInt(offset);
		ms.WriteEncodeInt(len);

		debug_printf("读写指令错误");
	}
	else
	{
		if(len > remain) len = remain;
		// 保存一份到缓冲区
		if(len > 0)
		{
			dv.Store.Copy(ms.Current(), len, offset);

			// 指针归零，准备写入响应数据
			ms.SetPosition(0);
			ms.WriteEncodeInt(offset);
			// 实际写入的长度
			ms.WriteEncodeInt(len);

			debug_printf("读写指令转换");
		}
	}
	msg.Length	= ms.Position();
	msg.Reply	= true;
	msg.Show();

	return true;
}

Device* TinyServer::FindDevice(byte id)
{
	if(id == 0) return NULL;

	for(int i=0; i<Devices.Length(); i++)
	{
		if(id == Devices[i]->Address) return Devices[i];
	}

	return NULL;
}

Device* TinyServer::FindDevice(const Array& hardid)
{
	if(hardid.Length() == 0) return NULL;

	for(int i=0; i<Devices.Length(); i++)
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
		//Devices.Remove(dv);
		int idx = Devices.FindIndex(dv);
		if(idx >= 0) Devices[idx] = NULL;
		delete dv;
		SaveDevices();
		return true;
	}

	return false;
}

int TinyServer::LoadDevices()
{
	debug_printf("TinyServer::LoadDevices 加载设备！\r\n");
	// 最后4k的位置作为存储位置
	uint addr = 0x8000000 + (Sys.FlashSize << 10) - (4 << 10);
	Flash flash;
	Config cfg(&flash, addr);

	byte* data = (byte*)cfg.Get("Devs");
	if(!data) return 0;

	Stream ms(data, 4 << 10);
	// 设备个数
	int count = ms.ReadByte();
	int i = 0;
	for(; i<count; i++)
	{
		byte id = ms.Peek();
		Device* dv = FindDevice(id);
		if(!dv) dv = new Device();
		dv->Read(ms);

		//Devices.Add(dv);
		int idx = Devices.FindIndex(NULL);
		if(idx >= 0) Devices[idx] = dv;
	}

	debug_printf("TinyServer::LoadDevices 从 0x%08X 加载 %d 个设备！\r\n", addr, i);

	return i;
}

void TinyServer::SaveDevices()
{
	// 最后4k的位置作为存储位置
	uint addr = 0x8000000 + (Sys.FlashSize << 10) - (4 << 10);
	Flash flash;
	Config cfg(&flash, addr);

	byte buf[0x800];

	Stream ms(buf, ArrayLength(buf));
	// 设备个数
	int count = Devices.Length();
	ms.Write((byte)count);
	for(int i = 0; i<count; i++)
	{
		Device* dv = Devices[i];
		dv->Write(ms);
	}
	debug_printf("TinyServer::SaveDevices 保存 %d 个设备到 0x%08X！\r\n", count, addr);
	cfg.Set("Devs", Array(ms.GetBuffer(), ms.Position()));
}

void TinyServer::ClearDevices()
{
	// 最后4k的位置作为存储位置
	uint addr = 0x8000000 + (Sys.FlashSize << 10) - (4 << 10);
	Flash flash;
	Config cfg(&flash, addr);

	debug_printf("TinyServer::ClearDevices 清空设备列表 0x%08X \r\n", addr);

	cfg.Invalid("Devs");

	for(int i=0; i<Devices.Length(); i++)
		delete Devices[i];
	Devices.SetLength(0);
}

bool TinyServer::LoadConfig()
{
	debug_printf("TinyServer::LoadDevices 加载设备！\r\n");
	// 最后4k的位置作为存储位置
	uint addr = 0x8000000 + (Sys.FlashSize << 10) - (4 << 10);
	Flash flash;
	Config cfg(&flash, addr);

	byte* data = (byte*)cfg.Get("TCfg");
	if(!data)
	{
		SaveConfig();	// 没有就保存默认配置
		return true;
	}

	Stream ms(data, sizeof(TinyConfig));
	// 读取配置信息
	Cfg->Read(ms);

	debug_printf("TinyServer::LoadConfig 从 0x%08X 加载成功 ！\r\n", addr);

	return true;
}

void TinyServer::SaveConfig()
{
	// 最后4k的位置作为存储位置
	uint addr = 0x8000000 + (Sys.FlashSize << 10) - (4 << 10);
	Flash flash;
	Config cfg(&flash, addr);

	byte buf[sizeof(TinyConfig)];

	Stream ms(buf, ArrayLength(buf));
	Cfg->Write(ms);

	debug_printf("TinyServer::SaveConfig 保存到 0x%08X！\r\n", addr);

	cfg.Set("TCfg", Array(ms.GetBuffer(), ms.Position()));
}

void TinyServer::ClearConfig()
{
	debug_printf("TinyServer::ClearDevices 设备区清零！\r\n");
	// 最后4k的位置作为存储位置
	uint addr = 0x8000000 + (Sys.FlashSize << 10) - (4 << 10);
	Flash flash;
	Config cfg(&flash, addr);

	debug_printf("TinyServer::ClearConfig 重置配置信息 0x%08X \r\n", addr);

	cfg.Invalid("TCfg");

	for(int i=0; i<Devices.Length(); i++)
		delete Devices[i];
	Devices.SetLength(0);
}
