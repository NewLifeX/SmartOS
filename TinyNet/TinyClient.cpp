#include "Time.h"
#include "Task.h"

#include "TinyClient.h"

#include "JoinMessage.h"

static bool OnClientReceived(Message& msg, void* param);

static void TinyClientTask(void* param);

/******************************** 初始化和开关 ********************************/

TinyClient::TinyClient(TinyController* control)
{
	assert_ptr(control);

	Control 	= control;

	Joining		= false;
	Server		= 0;
	Type		= Sys.Code;

	LastActive	= 0;

	Received	= NULL;
	Param		= NULL;

	Cfg			= NULL;

	_TaskID		= 0;

	NextReport	= 0;
}

void TinyClient::Open()
{
	Control->Received	= OnClientReceived;
	Control->Param		= this;

	Control->Mode = 0;	// 客户端只接收自己的消息
	Control->Open();

	TranID	= (int)Time.Current();

	_TaskID = Sys.AddTask(TinyClientTask, this, 0, 5000, "客户端服务");

	if(Cfg->Address > 0 && Cfg->Server > 0)
	{
		Control->Address = Cfg->Address;
		Server = Cfg->Server;

		Password.Load(Cfg->Password, ArrayLength(Cfg->Password));
	}
}

void TinyClient::Close()
{
	Sys.RemoveTask(_TaskID);

	Control->Received	= NULL;
	Control->Param		= NULL;

	Control->Close();
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

bool OnClientReceived(Message& msg, void* param)
{
	TinyClient* client = (TinyClient*)param;
	assert_ptr(client);

	client->OnReceive((TinyMessage&)msg);

	return true;
}

bool TinyClient::OnReceive(TinyMessage& msg)
{
	if(msg.Src == Server) LastActive = Time.Current();

	// 不处理来自网关以外的消息
	//if(Server == 0 || Server != msg.Dest) return true;
	if(msg.Code != 0x01 && Server != 0 && Server != msg.Src) return true;

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
	if(Received) return Received(msg, Param);

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

	Report(rs);//接受写入一次，刷新服务端

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
		ByteArray bs(ms.Current(), len);
		int count = Store.Write(offset, bs);
		ms2.WriteEncodeInt(count);
	}
	rs.Length	= ms2.Position();

	Reply(rs);
}

void TinyClient::Report(Message& msg)
{
	// 没有服务端时不要上报
	if(!Server) return;

	Stream ms = msg.ToStream();
	ms.Write((byte)0x01);	// 子功能码
	ms.Write((byte)0x00);	// 起始地址
	ms.Write((byte)Store.Data.Length());	// 长度
	ms.Write(Store.Data);
	msg.Length = ms.Position();
}

bool TinyClient::Report(uint offset, byte dat)
{
	TinyMessage msg;
	msg.Code	= 0x05;

	Stream ms = msg.ToStream();
	ms.WriteEncodeInt(offset);
	ms.Write(dat);
	msg.Length	= ms.Position();

	return Reply(msg);
}

bool TinyClient::Report(uint offset, const ByteArray& bs)
{
	TinyMessage msg;
	msg.Code	= 0x05;

	Stream ms = msg.ToStream();
	ms.WriteEncodeInt(offset);
	ms.Write(bs);
	msg.Length	= ms.Position();

	return Reply(msg);
}

void TinyClient::ReportAsync(uint offset)
{
	NextReport = offset;

	// 延迟200ms上报，期间有其它上报任务到来将会覆盖
	Sys.SetTask(_TaskID, true, 200);
}

/******************************** 常用系统级消息 ********************************/

void TinyClientTask(void* param)
{
	assert_ptr(param);

	TinyClient* client = (TinyClient*)param;
	uint offset = client->NextReport;
	assert_param2(offset == 0 || offset < 0x10, "自动上报偏移量异常！");
	if(offset)
	{
		client->Report(offset, client->Store.Data[offset]);
		client->NextReport = 0;
		return;
	}
	if(client->Server == 0 || client->Joining) client->Join();
	if(client->Server != 0) client->Ping();
}

// 发送发现消息，告诉大家我在这
// 格式：2设备类型 + N系统ID
void TinyClient::Join()
{
	TinyMessage msg;
	msg.Code = 1;

	// 发送的广播消息，设备类型和系统ID
	JoinMessage dm;
	dm.Kind		= Type;
	dm.HardID	= Sys.ID;
	dm.TranID	= TranID;
	dm.WriteMessage(msg);
	dm.Show(true);

	Control->Broadcast(msg);
}

// 组网
bool TinyClient::OnJoin(const TinyMessage& msg)
{
	// 客户端只处理Discover响应
	if(!msg.Reply || msg.Error) return true;

	// 解析数据
	JoinMessage dm;
	dm.ReadMessage(msg);
	dm.Show(true);

	// 校验不对
	if(TranID != dm.TranID)
	{
		debug_printf("发现响应序列号 0x%08X 不等于内部序列号 0x%08X \r\n", dm.TranID, TranID);
		//return true;
	}

	Joining		= false;

	Control->Address	= dm.Address;
	Password	= dm.Password;
	Password.Save(Cfg->Password, ArrayLength(Cfg->Password));

	// 记住服务端地址
	Server = dm.Server;
	Cfg->Channel	= dm.Channel;
	Cfg->Speed		= dm.Speed == 0 ? 250 : (dm.Speed == 1 ? 1000 : 2000);

	// 服务端组网密码，退网使用
	Cfg->ServerKey[0] = dm.HardID.Length();
	dm.HardID.Save(Cfg->ServerKey, ArrayLength(Cfg->ServerKey));

	debug_printf("组网成功！\r\n");
	//debug_printf("组网成功！由网关 0x%02X 分配得到节点地址 0x%02X ，频道：%d，传输速率：%dkbps，密码：", Server, dm.Address, dm.Channel, Cfg->Speed);

	// 取消Join任务，启动Ping任务
	ushort time		= Cfg->PingTime;
	if(time < 5)	time	= 5;
	if(time > 60)	time	= 60;
	Cfg->PingTime	= time;
	Sys.SetTaskPeriod(_TaskID, time * 1000);

	// 组网成功更新一次最后活跃时间
	LastActive = Time.Current();

	// 保存配置
	//Cfg->Save();
	
	return true;
}

// 离网
bool TinyClient::OnDisjoin(const TinyMessage& msg)
{
	return true;
}

// 心跳
void TinyClient::Ping()
{
	ushort off = Cfg->OfflineTime;
	if(off < 10) off = 10;
	if(LastActive > 0 && LastActive + off * 1000 < Time.Current())
	{
		if(Server == 0) return;

		debug_printf("%d 秒无法联系，服务端可能已经掉线，重启Join任务，关闭Ping任务\r\n", off);

		Sys.SetTaskPeriod(_TaskID, 5000);

		// 掉线以后，重发组网信息，基本功能继续执行
		Joining	= true;

		//Server		= 0;
		//Password.SetLength(0);

		//return;
	}

	TinyMessage msg;
	msg.Code = 3;

	// 没事的时候，心跳指令承载0x01子功能码，作为数据上报
	Report(msg);

	Send(msg);

	if(LastActive == 0) LastActive = Time.Current();
}

bool TinyClient::OnPing(const TinyMessage& msg)
{
	// 仅处理来自网关的消息
	if(Server == 0 || Server != msg.Dest) return true;

	// 忽略响应消息
	if(msg.Reply)
	{
		if(msg.Src == Server) LastActive = Time.Current();
		return true;
	}

	debug_printf("TinyClient::OnPing Length=%d\r\n", msg.Length);

	return true;
}
/*
// 系统时间获取与设置
bool TinyClient::SysTime(Message& msg, void* param)
{
	// 忽略响应消息
	if(msg.Reply) return true;

	debug_printf("Message_SysTime Length=%d\r\n", msg.Length);

	// 负载数据决定是读时间还是写时间
	ByteArray bs(msg.Data, msg.Length);
	if(msg.Length >= 8)
	{
		// 写时间
		ulong us = bs.ToUInt64();

		Time.SetTime(us);
	}

	// 读时间
	ulong us2 = Time.Current();
	msg.Length = 8;
	bs.Write(us2);

	return true;
}

bool TinyClient::SysID(Message& msg, void* param)
{
	TinyMessage& tmsg = (TinyMessage&)msg;
	// 忽略响应消息
	if(tmsg.Reply) return true;

	debug_printf("Message_SysID Length=%d\r\n", msg.Length);

	if(msg.Length == 0)
	{
		// 12字节ID，4字节CPUID，4字节设备ID
		msg.SetData(Sys.ID, 5 << 2);
	}
	else
	{
		// 干脆直接输出Sys，前面11个uint
		msg.SetData((byte*)&Sys, 11 << 2);
	}

	return true;
}

bool TinyClient::SysMode(Message& msg, void* param)
{
	TinyMessage& tmsg = (TinyMessage&)msg;
	// 忽略响应消息
	if(tmsg.Reply) return true;

	byte mode = 0;
	if(msg.Length > 0) mode = msg.Data[0];

	debug_printf("Message_SysMode Length=%d Mode=%d\r\n", msg.Length, mode);

	switch(mode)
	{
		case 1:	// 系统软重启
			Sys.Reset();
			break;
	}

	msg.Length = 1;
	msg.Data[0] = 0;

	return true;
}*/
