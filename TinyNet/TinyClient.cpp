#include "Kernel\TTime.h"

#include "TinyClient.h"
#include "Security\Crc.h"

#include "JoinMessage.h"
#include "PingMessage.h"
#include "DataMessage.h"

TinyClient* TinyClient::Current	= nullptr;

static void TinyClientTask(void* param);
//static void TinyClientReset();
//static void GetDeviceKey(byte id, Buffer& key, void* param);

/******************************** 初始化和开关 ********************************/

TinyClient::TinyClient(TinyController* control)
{
	Control 		= control;
	Control->GetKey	= Delegate2<byte, Buffer&>(&TinyClient::GetDeviceKey, this);

	Opened		= false;
	Joining		= false;
	Server		= 0;
	Type		= Sys.Code;

	LastSend	= 0;
	LastActive	= 0;

	Received	= nullptr;
	Param		= nullptr;

	Cfg			= nullptr;

	_TaskID		= 0;

	NextReport	= 0;
	Encryption	= false;
}

void TinyClient::Open()
{
	if(Opened) return;

	// 使用另一个强类型参数的委托，事件函数里面不再需要做类型
	Control->Received	= Delegate2<TinyMessage&, TinyController&>(&TinyClient::OnReceive, this);
	//Control->Param		= this;

	TranID	= (int)Sys.Ms();

	if(Cfg->Address > 0 && Cfg->Server > 0)
	{
		Control->Address = Cfg->Address;
		Server = Cfg->Server;

		//Password.Load(Cfg->Password, ArrayLength(Cfg->Password));
		Password	= Cfg->Pass;
	}

	HardCrc	= Crc::Hash16(Buffer(Sys.ID, 16));
	if(Sys.Ver > 1) Encryption = true;

	Control->Mode = 0;	// 客户端只接收自己的消息
	Control->Open();

	int t	= 5000;
	if(Server) t	= Cfg->PingTime * 1000;
	if(t < 1000) t = 1000;
	_TaskID = Sys.AddTask(TinyClientTask, this, 0, t, "微网客户端");

	Opened	= true;
}

void TinyClient::Close()
{
	if(!Opened) return;

	Sys.RemoveTask(_TaskID);

	//Control->Received	= nullptr;
	//Control->Param		= nullptr;

	Control->Close();

	Opened	= false;
}

/******************************** 收发中心 ********************************/
bool TinyClient::Send(TinyMessage& msg)
{
	//assert(this, "令牌客户端未初始化");
	assert(Control, "令牌控制器未初始化");

	// 未组网时，禁止发其它消息。组网消息通过广播发出，不经过这里
	if(!Server) return false;

	// 设置网关地址
	if(!msg.Dest) msg.Dest = Server;

	return Control->Send(msg);
}

bool TinyClient::Reply(TinyMessage& msg)
{
	//assert(this, "令牌客户端未初始化");
	assert(Control, "令牌控制器未初始化");

	// 未组网时，禁止发其它消息。组网消息通过广播发出，不经过这里
	if(!Server) return false;

	if(!msg.Dest) msg.Dest = Server;

	return Control->Reply(msg);
}

void TinyClient::OnReceive(TinyMessage& msg, TinyController& ctrl)
{
	// 不是组网消息。不是被组网网关消息，不受其它消息设备控制.
	if(msg.Code != 0x01 && Server != msg.Src) return;

	if(msg.Src == Server) LastActive = Sys.Ms();

	switch(msg.Code)
	{
		case 0x01:
			OnJoin(msg);
			break;
		case 0x02:
			OnDisjoin(msg);
			break;
		case 0x03:
			OnPing(msg);
			break;
		case 0x05:
		case 0x15:
			OnRead(msg);
			break;
		case 0x06:
		case 0x16:
			OnWrite(msg);
			break;
	}

	// 消息转发
	if(Received) Received(this, msg, Param);
}

/******************************** 数据区 ********************************/
/*
请求：1起始 + 1大小
响应：1起始 + N数据
错误：错误码2 + 1起始 + 1大小
*/
void TinyClient::OnRead(const TinyMessage& msg)
{
	if(msg.Reply) return;
	if(msg.Length < 2) return;

	auto rs	= msg.CreateReply();
	auto ms	= rs.ToStream();

	DataMessage dm(msg, &ms);

	bool rt	= true;
	if(dm.Offset < 64)
		rt	= dm.ReadData(Store);
	else if(dm.Offset < 128)
	{
		dm.Offset	-= 64;
		Buffer bs(Cfg, Cfg->Length);
		rt	= dm.ReadData(bs);
	}

	rs.Error	= !rt;
	rs.Length	= ms.Position();

	Reply(rs);
}

/*
请求：1起始 + N数据
响应：1起始 + 1大小 + N数据
错误：错误码2 + 1起始 + 1大小
*/
void TinyClient::OnWrite(const TinyMessage& msg)
{
	if(msg.Reply) return;
	if(msg.Length < 2) return;

	auto rs	= msg.CreateReply();
	auto ms	= rs.ToStream();

	DataMessage dm(msg, &ms);

	bool rt	= true;
	if(dm.Offset < 64)
	{
		rt	= dm.WriteData(Store, true);
	}
	else if(dm.Offset < 128)
	{
		dm.Offset	-= 64;
		Buffer bs(Cfg, Cfg->Length);
		rt	= dm.WriteData(bs, true);

		Cfg->Save();
	}

	rs.Error	= !rt;
	rs.Length	= ms.Position();

	Reply(rs);

	if(dm.Offset >= 64 && dm.Offset < 128)
	{
		debug_printf("\r\n 配置区被修改，200ms后重启\r\n");
		Sys.Sleep(200);
		Sys.Reboot();
	}

	// 写入指令以后，为了避免写入响应丢失，缩短心跳间隔
	Sys.SetTask(_TaskID, true, 500);
}

bool TinyClient::Report(uint offset, byte dat)
{
	TinyMessage msg;
	msg.Code	= 0x06;

	auto ms = msg.ToStream();
	ms.WriteEncodeInt(offset);
	ms.Write(dat);
	msg.Length	= ms.Position();

	return Send(msg);
}

bool TinyClient::Report(uint offset, const Buffer& bs)
{
	TinyMessage msg;
	msg.Code	= 0x06;

	auto ms = msg.ToStream();
	ms.WriteEncodeInt(offset);
	ms.Write(bs);
	msg.Length	= ms.Position();

	return Send(msg);
}

void TinyClient::ReportAsync(uint offset,uint length)
{
	//if(this == nullptr) return;
	if(offset + length >= Store.Data.Length()) return;

	NextReport = offset;
	ReportLength = length;
	// 延迟200ms上报，期间有其它上报任务到来将会覆盖
	Sys.SetTask(_TaskID, true, 200);
}

/******************************** 常用系统级消息 ********************************/

void TinyClientTask(void* param)
{
	auto client = (TinyClient*)param;
	uint offset = client->NextReport;
	uint len	= client->ReportLength;
	assert(offset == 0 || offset < 0x10, "自动上报偏移量异常！");

	if(offset)
	{
		// 检查索引，否则数组越界
		auto& bs = client->Store.Data;
		if(bs.Length() > offset + len)
		{
			if(len == 1)
				client->Report(offset, bs[offset]);
			else
			{
				auto bs2 = ByteArray(&bs[offset], len);
				client->Report(offset, bs2);
			}
		}
		client->NextReport = 0;
		return;
	}
	if(client->Server == 0 || client->Joining) client->Join();
	if(client->Server != 0) client->Ping();
}

void TinyClient::GetDeviceKey(byte id, Buffer& key)
{
	TS("TinyClient::GetDeviceKey");

	//if(Sys.Version < 0xFFFF) return;

	//key	= Password;
	//key.Copy(Password, 8);
}

// 组网消息，告诉大家我在这
void TinyClient::Join()
{
	TS("TinyClient::Join");

	TinyMessage msg;
	msg.Code = 1;

	// 发送的广播消息，设备类型和系统ID
	JoinMessage dm;

	// 组网版本不是系统版本，而是为了做新旧版本组网消息兼容的版本号
	//dm.Version	= Sys.Version;
	dm.Kind		= Type;
	//dm.HardID.Copy(Sys.ID, 16);
	dm.HardID	= Sys.ID;
	dm.TranID	= TranID;
	dm.WriteMessage(msg);
	//dm.Show(true);

	Control->Broadcast(msg);
}

// 组网
bool TinyClient::OnJoin(const TinyMessage& msg)
{
	// 客户端只处理Discover响应
	if(!msg.Reply || msg.Error) return true;

	TS("TinyClient::OnJoin");

	// 解析数据
	JoinMessage dm;
	dm.ReadMessage(msg);
	dm.Show(true);

	// 校验不对
	if(TranID != dm.TranID)
	{
		debug_printf("组网响应序列号 0x%04X 不等于内部序列号 0x%04X \r\n", dm.TranID, TranID);
		return true;
	}

	Joining		= false;

	Cfg->SoftVer	= dm.Version;
	// 小于2的版本不加密
	if(dm.Version < 2) Encryption	= false;

	Cfg->Address	= dm.Address;
	Control->Address	= dm.Address;
	Password	= dm.Password;
	Cfg->Pass	= dm.Password;
	//Password.Copy(0, dm.Password, 0, -1);
	//Password.Save(Cfg->Password, ArrayLength(Cfg->Password));

	// 记住服务端地址
	Server = dm.Server;
	Cfg->Server		= dm.Server;
	if(Cfg->Channel != dm.Channel)
	{
	   //todo 设置zigbee 通道
	}
	Cfg->Channel	= dm.Channel;
	Cfg->Speed		= dm.Speed * 10;

	// 服务端组网密码，退网使用
	//Cfg->Mac[0]		= dm.HardID.Length();
	//dm.HardID.Save(Cfg->Mac, ArrayLength(Cfg->Mac));
	dm.HardID.CopyTo(0, Cfg->Mac, -1);

#if DEBUG
	debug_printf("组网成功！网关 0x%02X 分配 0x%02X ，频道：%d，传输速率：%dkbps，密码：", dm.Server, dm.Address, dm.Channel, Cfg->Speed);
	Password.Show();
	debug_printf(", 版本: 0x%02X\r\n", dm.Version);
#endif

	// 取消Join任务，启动Ping任务
	ushort time		= Cfg->PingTime;
	if(time < 5)	time	= 5;
	if(time > 60)	time	= 60;
	Cfg->PingTime	= time;
	Sys.SetTaskPeriod(_TaskID, time * 1000);

	// 组网成功更新一次最后活跃时间
	LastActive = Sys.Ms();

	// 保存配置
	Cfg->Save();

	// 组网以后重启一次
	Sys.Reboot();

	return true;
}

void TinyClient::DisJoin()
{
	TS("TinyClient::DisJoin");

	TinyMessage msg;
	msg.Code	= 2;

	auto ms		= msg.ToStream();
	ms.Write(HardCrc);
	msg.Length	= ms.Position();

	Send(msg);
}

// 离网
bool TinyClient::OnDisjoin(const TinyMessage& msg)
{
	if(msg.Length < 2) return false;

	TS("TinyClient::OnDisJoin");

	auto ms		= msg.ToStream();
	ushort crc	= ms.ReadUInt16();

	if(crc != HardCrc)
	{
		debug_printf("退网密码 0x%04X 不等于本地密码 0x%04X \r\n", crc, HardCrc);
		return false;
	}

	Cfg->Clear();

    Sys.Sleep(3000);
    Sys.Reboot();

	return true;
}

// 心跳
void TinyClient::Ping()
{
	TS("TinyClient::Ping");

	/*ushort off = (Cfg->OfflineTime)*5;
	//debug_printf(" TinyClient::Ping  Cfg->OfflineTime:%d\r\n", Cfg->OfflineTime);
    ushort now = Sys.Seconds();

	if(off < 10) off = 30;

	if(LastActive > 0 && LastActive + off * 1000 < Sys.Ms())
	{
		if(Server == 0) return;

		debug_printf(" %d 秒无法联系网关，最后活跃时间: %d ,系统当前时间:%d \r\n", off, (int)LastActive, (int)now);
		Sys.SetTaskPeriod(_TaskID, 5000);

		// 掉线以后，重发组网信息，基本功能继续执行
		Joining	= true;

		//auto tc	= TinyConfig::Current;
		//memset(tc->Mac, 0, 5);
		//tc->Clear();
		//Server = 0;


		//Sys.Reboot();
		//Server	= 0;
		//Password.SetLength(0);

		//return;
	}*/

	debug_printf("TinyClient::Ping");
	// 没有服务端时不要上报
	if(!Server) return;

	// 30秒内发过数据，不再发送心跳
	if(LastSend > 0 && LastSend + 30000 > Sys.Ms()) return;

	TinyMessage msg;
	msg.Code = 3;

	auto ms = msg.ToStream();
	PingMessage pm;
	pm.MaxSize	= ms.Capacity();
	uint len = Control->Port->MaxSize - TinyMessage::MinSize;
	if(pm.MaxSize > len) pm.MaxSize = len;

	pm.WriteData(ms, 0x01, Store.Data);
	pm.WriteHardCrc(ms, HardCrc);
	pm.WriteData(ms, 0x02, Buffer(Cfg, sizeof(Cfg[0])));

	msg.Length = ms.Position();

	Send(msg);
}

bool TinyClient::OnPing(const TinyMessage& msg)
{
	// 仅处理来自网关的消息
	if(Server == 0 || Server != msg.Src) return true;

	TS("TinyClient::OnPing");

	// 忽略响应消息
	if(!msg.Reply)return true;

	if(msg.Src != Server) return true;

	// 处理消息
	auto ms	= msg.ToStream();
	PingMessage pm;
	pm.MaxSize	= Control->Port->MaxSize - TinyMessage::MinSize;
	// 子操作码
	while(ms.Remain())
	{
		switch(ms.ReadByte())
		{
			case 0x04:
			{
				uint seconds = 0;
				if(pm.ReadTime(ms, seconds))
				{
					((TTime&)Time).SetTime(seconds);
				}
				break;
			}
			default:
				break;
		}
	}

	return true;
}
