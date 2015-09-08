#include "Time.h"
#include "Task.h"
#include "Config.h"

#include "TinyClient.h"

#include "JoinMessage.h"

static bool OnClientReceived(Message& msg, void* param);

static void TinyClientTask(void* param);

/******************************** 初始化和开关 ********************************/

TinyClient::TinyClient(TinyController* control)
{
	assert_ptr(control);

	Control 	= control;

	Server		= 0;
	Type		= Sys.Code;

	LastActive	= 0;

	Received	= NULL;
	Param		= NULL;

	Config		= NULL;
	
	_TaskID		= 0;
}

void TinyClient::Open()
{
	Control->Received	= OnClientReceived;
	Control->Param		= this;

	Control->Open();

	TranID	= (int)Time.Current();

	_TaskID = Sys.AddTask(TinyClientTask, this, 0, 5000000, "客户端服务");

	if(Config->Address > 0 && Config->Server > 0)
	{
		Control->Address = Config->Address;
		Server = Config->Server;
		
		Password.Load(Config->Password, ArrayLength(Config->Password));
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
	if(Server != 0 && Server != msg.Src) return true;

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
错误：1起始
*/
void TinyClient::OnRead(const TinyMessage& msg)
{
	if(msg.Reply) return;
	if(msg.Length < 2) return;

	// 起始地址为7位压缩编码整数
	Stream ms(msg.Data, msg.Length);
	uint offset = ms.ReadEncodeInt();
	uint len	= ms.ReadEncodeInt();

	// 准备响应数据
	TinyMessage rs;
	rs.Code	= msg.Code;
	Stream ms2(rs.Data, ArrayLength(rs._Data));
	ms2.WriteEncodeInt(offset);

	if(len > ms2.Remain()) len = ms2.Remain();
	ByteArray bs(ms2.Current(), len);
	int count = Store.Read(offset, bs);

	if(count < 0)
	{
		// 出错，使用原来的数据区即可，只需要返回一个起始位置
		rs.Error = true;
	}
	else
		ms2.Seek(count);
	//rs.Length = ms2.Position();
	rs.SetData(ms2.GetBuffer(), ms2.Position());

	Reply(rs);
}

/*
请求：1起始 + N数据
响应：1起始 + 1大小
错误：1起始
*/
void TinyClient::OnWrite(const TinyMessage& msg)
{
	if(msg.Reply) return;
	if(msg.Length < 2) return;

	// 起始地址为7位压缩编码整数
	Stream ms(msg.Data, msg.Length);
	uint offset = ms.ReadEncodeInt();

	ByteArray bs(ms.Current(), ms.Remain());
	int count = Store.Write(offset, bs);

	// 准备响应数据
	TinyMessage rs;
	rs.Code	= msg.Code;
	Stream ms2(rs.Data, ArrayLength(rs._Data));
	ms2.WriteEncodeInt(offset);

	if(count < 0)
		rs.Error = true;
	else
		ms2.WriteEncodeInt(count);

	rs.Length = ms2.Position();

	Reply(rs);
}

void TinyClient::Report(TinyMessage& msg)
{
	// 没有服务端时不要上报
	if(!Server) return;

	Stream ms(msg.Data, ArrayLength(msg._Data));
	ms.Write((byte)0x01);	// 子功能码
	ms.Write((byte)0x00);	// 起始地址
	ms.Write(Store.Data);
	msg.Length = ms.Position();
}

/******************************** 常用系统级消息 ********************************/

void TinyClientTask(void* param)
{
	assert_ptr(param);

	TinyClient* client = (TinyClient*)param;
	if(client->Server == 0)
		client->Join();
	else
		client->Ping();
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

	Control->Address	= dm.Address;
	Password	= dm.Password;
	Password.Save(Config->Password, ArrayLength(Config->Password));

	// 记住服务端地址
	Server = dm.Server;
	Config->Channel	= dm.Channel;
	Config->Speed	= dm.Speed == 0 ? 250 : (dm.Speed == 1 ? 1000 : 2000);

	// 服务端组网密码，退网使用
	Config->ServerKey[0] = dm.HardID.Length();
	dm.HardID.Save(Config->ServerKey, ArrayLength(Config->ServerKey));

	debug_printf("组网成功！由网关 0x%02X 分配得到节点地址 0x%02X ，频道：%d，传输速率：%dkbps，密码：", Server, dm.Address, dm.Channel, Config->Speed);
	Password.Show(true);

	// 取消Join任务，启动Ping任务
	ushort time = Config->PingTime;
	if(time < 5) time = 5;
	Sys.SetTaskPeriod(_TaskID, time * 1000000);

	// 组网成功更新一次最后活跃时间
	LastActive = Time.Current();

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
	ushort off = Config->OfflineTime;
	if(off < 10) off = 10;
	if(LastActive > 0 && LastActive + off * 1000000 < Time.Current())
	{
		if(Server == 0) return;

		debug_printf("%d 秒无法联系，服务端可能已经掉线，重启Join任务，关闭Ping任务\r\n", off);

		Sys.SetTaskPeriod(_TaskID, 5000000);

		Server		= 0;
		Password	= 0;

		return;
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
