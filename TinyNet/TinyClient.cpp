#include "Time.h"

#include "TinyClient.h"
#include "Security\Crc.h"

#include "JoinMessage.h"
#include "PingMessage.h"

TinyClient* TinyClient::Current	= NULL;

static void TinyClientTask(void* param);
static void TinyClientReset();
static void GetDeviceKey(byte id, Array& key, void* param);

/******************************** 初始化和开关 ********************************/

TinyClient::TinyClient(TinyController* control)
{
	assert_ptr(control);

	Control 		= control;
	Control->GetKey	= GetDeviceKey;

	Opened		= false;
	Joining		= false;
	Server		= 0;
	Type		= Sys.Code;

	LastActive	= 0;

	Received	= NULL;
	Param		= NULL;

	Cfg			= NULL;

	_TaskID		= 0;

	NextReport	= 0;
	Encryption	= false;
}

void TinyClient::Open()
{
	if(Opened) return;

	Control->Received	= [](void* s, Message& msg, void* p){ return ((TinyClient*)p)->OnReceive((TinyMessage&)msg); };
	Control->Param		= this;

	TranID	= (int)Sys.Ms();

	_TaskID = Sys.AddTask(TinyClientTask, this, 0, 5000, "客户端服务");

	if(Cfg->Address > 0 && Cfg->Server > 0)
	{
		Control->Address = Cfg->Address;
		Server = Cfg->Server;

		Password.Load(Cfg->Password, ArrayLength(Cfg->Password));
	}

	HardCrc	= Crc::Hash16(Array(Sys.ID, 16));
	if(Sys.Version > 1) Encryption = true;

	Control->Mode = 0;	// 客户端只接收自己的消息
	Control->Open();

	Opened	= true;
}

void TinyClient::Close()
{
	if(!Opened) return;

	Sys.RemoveTask(_TaskID);

	Control->Received	= NULL;
	Control->Param		= NULL;

	Control->Close();

	Opened	= false;
}

/******************************** 收发中心 ********************************/
bool TinyClient::Send(TinyMessage& msg)
{
	assert_param2(this, "令牌客户端未初始化");
	assert_param2(Control, "令牌控制器未初始化");

	// 未组网时，禁止发其它消息。组网消息通过广播发出，不经过这里
	if(!Server) return false;

	// 设置网关地址
	if(!msg.Dest) msg.Dest = Server;

	return Control->Send(msg);
}

bool TinyClient::Reply(TinyMessage& msg)
{
	assert_param2(this, "令牌客户端未初始化");
	assert_param2(Control, "令牌控制器未初始化");

	// 未组网时，禁止发其它消息。组网消息通过广播发出，不经过这里
	if(!Server) return false;

	if(!msg.Dest) msg.Dest = Server;

	return Control->Reply(msg);
}

bool TinyClient::OnReceive(TinyMessage& msg)
{
	// 不处理来自网关以外的消息
	//if(Server == 0 || Server != msg.Dest) return true;
	//debug_printf("源地址: 0x%08X 网关地址:0x%08X \r\n",Server, msg.Src);

	// 不是组网消息。不是被组网网关消息，不受其它消息设备控制.
	if(msg.Code != 0x01 && Server != msg.Src) return true;

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
	if(Received) return Received(this, msg, Param);

	return true;
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

	// 起始地址为7位压缩编码整数
	Stream ms	= msg.ToStream();
	uint offset = ms.ReadEncodeInt();

	if(ReadCfg(offset,ms)) return;

	uint len	= ms.ReadEncodeInt();

	// 准备响应数据
	TinyMessage rs;
	rs.Code		= msg.Code;
	Stream ms2	= rs.ToStream();

	ByteArray& bs = Store.Data;

	int remain = bs.Length() - offset;
	if(remain < 0)
	{
		rs.Error = true;
		ms2.Write((byte)2);
		ms2.WriteEncodeInt(offset);
		ms2.WriteEncodeInt(len);
	}
	else
	{
		ms2.WriteEncodeInt(offset);
		if(len > remain) len = remain;
		if(len > 0) ms2.Write(bs.GetBuffer(), offset, len);
	}
	rs.Length	= ms2.Position();

	Reply(rs);

	//Report(rs);//接受写入一次，刷新服务端
}

bool TinyClient::ReadCfg(uint offset,	Stream ms)
{
	if(offset < Cfg->StartSet) return false;

	//响应一条数据
	TinyMessage rs;
	rs.Code		= 0x15;
	Stream ms2	= rs.ToStream();

	ByteArray cfg(Cfg, Cfg->Length);
	uint adrr=offset-Cfg->StartSet;
	uint len = ms.Remain();

    if((adrr+len)>Cfg->Length)
	{
		rs.Error = true;
		ms2.Write((byte)2);
		ms2.WriteEncodeInt(offset);
		ms2.WriteEncodeInt(len);
	}
	else
	{
		ms2.WriteEncodeInt(offset);
		if(len > 0) ms2.Write(cfg.GetBuffer(), offset, len);
	}
	rs.Length	= ms2.Position();

	Reply(rs);
	return true;
}
/*
请求：1起始 + N数据
响应：1起始 + 1大小
错误：错误码2 + 1起始 + 1大小
*/
void TinyClient::OnWrite(const TinyMessage& msg)
{
	if(msg.Reply) return;
	if(msg.Length < 2) return;

	// 起始地址为7位压缩编码整数
	Stream ms	= msg.ToStream();
	uint offset = ms.ReadEncodeInt();

	if(WriteCfg(offset,ms)) return;
	// 准备响应数据
	TinyMessage rs;
	rs.Code		= msg.Code;
	Stream ms2	= rs.ToStream();

	// 剩余可写字节数
	uint len = ms.Remain();
	int remain = Store.Data.Length() - offset;
	if(remain < 0)
	{
		rs.Error = true;
		ms2.Write((byte)2);
		ms2.WriteEncodeInt(offset);
		ms2.WriteEncodeInt(len);

	}
	else
	{
		ms2.WriteEncodeInt(offset);

		if(len > remain) len = remain;
		Array bs(ms.Current(), len);
		int count = Store.Write(offset, bs);
		ms2.WriteEncodeInt(count);
	}
	rs.Length	= ms2.Position();

	Reply(rs);
}

bool TinyClient::WriteCfg(uint offset,	Stream ms)
{
	if(offset < Cfg->StartSet) return false;

	//响应一条数据
	TinyMessage rs;
	rs.Code		= 0x16;
	Stream ms2	= rs.ToStream();

	ByteArray cfg(Cfg, Cfg->Length);
	uint adrr=offset-Cfg->StartSet;
	uint len = ms.Remain();
    if((adrr+len)>Cfg->Length)
   	{
   		rs.Error = true;
   		ms2.Write((byte)2);
   		ms2.WriteEncodeInt(offset);
   		ms2.WriteEncodeInt(len);
		Reply(rs);
		return  true;//操作成功的意思
   	}

	Array bs(ms.Current(), len);
	bs.CopyTo(&cfg[adrr],len);

	Cfg->Save();

	ms2.WriteEncodeInt(offset);
	ms2.WriteEncodeInt(len);
	rs.Length	= ms2.Position();

	Reply(rs);

	TinyClientReset();
	//Sys.Reset();

	//修改配置区，重启系统
	//debug_printf("修改后的设备ID 0x%08X ,偏移量 %d\r\n", Cfg->Address,offset);
	//cfg.Show();
	return true;
}

void TinyClient::Report(Message& msg)
{
	TS("TinyMessage::Report");
	// 没有服务端时不要上报
	if(!Server) return;

	auto ms = msg.ToStream();
	//MemoryStream ms;
	//byte buf[256];
	//MemoryStream ms(buf, ArrayLength(buf));
	PingMessage pm;
	pm.MaxSize	= ms.Capacity();
	uint len = Control->Port->MaxSize - TinyMessage::MinSize;
	if(pm.MaxSize > len) pm.MaxSize = len;
	//pm.MaxSize = Control->Port->MaxSize - TinyMessage::MinSize;
	//debug_printf("Report rm=%d MaxSize=%d %d \r\n", ms.Remain(), pm.MaxSize, Control->Port->MaxSize);

	pm.WriteData(ms, 0x01, Store.Data);
	pm.WriteHardCrc(ms, HardCrc);
	pm.WriteData(ms, 0x02, Array(Cfg, sizeof(Cfg[0])));

	msg.Length = ms.Position();
}

bool TinyClient::Report(uint offset, byte dat)
{
	TinyMessage msg;
	msg.Code	= 0x05;

	auto ms = msg.ToStream();
	ms.WriteEncodeInt(offset);
	ms.Write(dat);
	msg.Length	= ms.Position();

	return Reply(msg);
}

bool TinyClient::Report(uint offset, const Array& bs)
{
	TinyMessage msg;
	msg.Code	= 0x05;

	auto ms = msg.ToStream();
	ms.WriteEncodeInt(offset);
	ms.Write(bs);
	msg.Length	= ms.Position();

	return Reply(msg);
}

void TinyClient::ReportAsync(uint offset)
{
	if(this == NULL) return;
	if(offset >= Store.Data.Length()) return;

	NextReport = offset;

	// 延迟200ms上报，期间有其它上报任务到来将会覆盖
	Sys.SetTask(_TaskID, true, 200);
}

/******************************** 常用系统级消息 ********************************/

void TinyClientTask(void* param)
{
	assert_ptr(param);

	auto client = (TinyClient*)param;
	uint offset = client->NextReport;
	assert_param2(offset == 0 || offset < 0x10, "自动上报偏移量异常！");
	if(offset)
	{
		// 检查索引，否则数组越界
		ByteArray& bs = client->Store.Data;
		if(bs.Length() > offset) client->Report(offset, bs[offset]);
		client->NextReport = 0;
		return;
	}
	if(client->Server == 0 || client->Joining) client->Join();
	if(client->Server != 0) client->Ping();
}

void TinyClientReset()
{
	//上报一条信息，让网关得一修改
	//Join();

	Sys.Reset();
}

void GetDeviceKey(byte id, Array& key, void* param)
{
	TS("TinyClient::GetDeviceKey");
	//debug_printf("微网客户端获取密钥");
	return;
	auto client = (TinyClient*)param;
	if(Sys.Version < 0xFFFF) return;

	//key = client->Password;
	key.Copy(client->Password, 8);
}

// 发送发现消息，告诉大家我在这
// 格式：2设备类型 + N系统ID
void TinyClient::Join()
{
	TS("TinyClient::Join");

	TinyMessage msg;
	msg.Code = 1;

	// 发送的广播消息，设备类型和系统ID
	JoinMessage dm;

	dm.Version	= Sys.Version;
	dm.Kind		= Type;
	dm.HardID.Copy(Sys.ID, 16);
	dm.TranID	= TranID;
	//dm.Name		= Cfg->Name;
	dm.WriteMessage(msg);
	dm.Show(true);

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
	// 小于2的版本加不加密
	if(dm.Version < 2) Encryption	= false;

	Cfg->Address	= dm.Address;
	Control->Address	= dm.Address;
	Password	= dm.Password;
	Password.Save(Cfg->Password, ArrayLength(Cfg->Password));

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
	dm.HardID.CopyTo(Cfg->Mac);

#if DEBUG
	//debug_printf("组网成功！\r\n");
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
	Sys.Reset();

	return true;
}

void TinyClient::DisJoin()
{
	TS("TinyClient::DisJoin");

	debug_printf("TinyClient::DisJoin 退网 \r\n");

	TinyMessage msg;
	msg.Code = 2;

	// 发送的广播消息，设备类型和系统ID
	JoinMessage dm;
	dm.Kind		= Type;
	dm.HardID.Set(Sys.ID, 16);
	dm.TranID	= TranID;
	dm.WriteMessage(msg);
	dm.Show(true);

	Send(msg);
}

// 离网
bool TinyClient::OnDisjoin(const TinyMessage& msg)
{
	if(msg.Length < 2) return false;

	TS("TinyClient::OnDisJoin");

	Stream ms(msg.Data, msg.Length);
	ushort crc	= ms.ReadUInt16();

	if(crc != HardCrc)
	{
		debug_printf("退网密码 0x%04X 不等于本地密码 0x%04X \r\n", crc, HardCrc);
		return false;
	}

	Cfg->LoadDefault();
	Cfg->Save();
    debug_printf("设备退网3秒后重启\r\n");

    Sys.Sleep(3000);
    Sys.Reset();
	return true;
}

// 心跳
void TinyClient::Ping()
{
	TS("TinyClient::Ping");

	ushort off = Cfg->OfflineTime;
	if(off < 10) off = 10;
	if(LastActive > 0 && LastActive + off * 1000 < Sys.Ms())
	{
		if(Server == 0) return;

		debug_printf("%d 秒无法联系网关，无线网可能已经掉线，重新组网，其它任务正常处理\r\n", off);

		Sys.SetTaskPeriod(_TaskID, 5000);

		// 掉线以后，重发组网信息，基本功能继续执行
		Joining	= true;

		auto tc	= TinyConfig::Current;
		memset(tc->Mac, 0, 5);
		tc->Save();
		Server = 0;
		

		Sys.Reset();
		Server		= 0;
		//Password.SetLength(0);

		//return;
	}

	TinyMessage msg;
	msg.Code = 3;

	Report(msg);

	Send(msg);

	if(LastActive == 0) LastActive = Sys.Ms();
}

bool TinyClient::OnPing(const TinyMessage& msg)
{
	// 仅处理来自网关的消息
	if(Server == 0 || Server != msg.Src) return true;

	TS("TinyClient::OnPing");

	// 忽略响应消息
	if(!msg.Reply)
	{
		debug_printf("TinyClient::OnPing Length=%d\r\n", msg.Length);

		return true;
	}

	if(msg.Src != Server) return true;

	//LastActive = Sys.Ms();

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
					Time.SetTime(seconds);
					//LastActive = Sys.Ms();
				}
				break;
			}
			default:
				break;
		}
	}

	return true;
}
