#include "Time.h"
#include "Task.h"
#include "TinyClient.h"

#include "JoinMessage.h"

bool OnClientReceived(Message& msg, void* param);

void TinyClientTask(void* param);

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

	_TaskID		= 0;
}

void TinyClient::Open()
{
	Control->Received	= OnClientReceived;
	Control->Param		= this;

	Control->Open();

	TranID	= (int)Time.Current();

	_TaskID = Sys.AddTask(TinyClientTask, this, 0, 5000000, "客户端服务");
}

void TinyClient::Close()
{
	if(_TaskID) Sys.RemoveTask(_TaskID);

	Control->Received	= NULL;
	Control->Param		= NULL;

	Control->Close();
}

/******************************** 收发中心 ********************************/

void TinyClient::Send(TinyMessage& msg)
{
	assert_param2(this, "令牌客户端未初始化");
	assert_param2(Control, "令牌控制器未初始化");
	
	// 设置网关地址
	if(!Server)return;
	if(!msg.Dest) msg.Dest = Server;
	
	Control->Send(msg);
}

void TinyClient::Reply(TinyMessage& msg)
{
	assert_param2(this, "令牌客户端未初始化");
	assert_param2(Control, "令牌控制器未初始化");

	Control->Reply(msg);
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
	if(Server != 0 && Server != msg.Dest) return true;

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
void TinyClient::OnRead(TinyMessage& msg)
{
	if(msg.Reply) return;
	if(msg.Length < 2) return;

	// 起始地址为7位压缩编码整数
	Stream ms = msg.ToStream();
	uint offset = ms.ReadEncodeInt();
	uint len	= ms.ReadEncodeInt();

	// 重新一个数据流，避免前面的不够
	Stream ms2(4 + len);
	ms2.WriteEncodeInt(offset);

	ByteArray bs(ms2.Current(), len);
	int rs = Store.Read(offset, bs);

	if(rs < 0)
	{
		// 出错，使用原来的数据区即可，只需要返回一个起始位置
		msg.Error = true;
		msg.Length = ms2.Position();
	}
	else
	{
		msg.SetData(ms2.GetBuffer(), ms2.Length);
	}

	Reply(msg);
}

/*
请求：1起始 + N数据
响应：1起始 + 1大小
错误：1起始
*/
void TinyClient::OnWrite(TinyMessage& msg)
{
	if(msg.Reply) return;
	if(msg.Length < 2) return;

	// 起始地址为7位压缩编码整数
	Stream ms = msg.ToStream();
	uint offset = ms.ReadEncodeInt();

	ByteArray bs(ms.Current(), ms.Remain());
	int rs = Store.Write(offset, bs);

	// 准备响应数据
	ms.Length = ms.Position();
	if(rs < 0)
		msg.Error = true;
	else
		ms.WriteEncodeInt(rs);

	msg.Length = ms.Length;

	Reply(msg);
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
bool TinyClient::OnJoin(TinyMessage& msg)
{
	// 客户端只处理Discover响应
	if(!msg.Reply || msg.Error) return true;

	// 校验不对
	if(TranID != msg.Sequence)
	{
		debug_printf("发现响应序列号 %d 不等于内部序列号 %d \r\n", msg.Sequence, TranID);
		//return true;
	}

	// 解析数据
	JoinMessage dm;
	dm.ReadMessage(msg);
	dm.Show(true);

	Control->Address	= dm.Address;
	Password	= dm.Password;

	// 记住服务端地址
	Server = dm.Server;

	debug_printf("组网成功！由网关 0x%02X 分配得到节点地址 0x%02X ，频道：%d，密码：", Server, dm.Address, 0);
	Password.Show(true);

	// 取消Join任务，启动Ping任务
	Task* task = Scheduler[_TaskID];
	task->Period = 15000000;

	return true;
}

// 离网
bool TinyClient::OnDisjoin(TinyMessage& msg)
{
	return true;
}

// 心跳
void TinyClient::Ping()
{
	if(LastActive > 0 && LastActive + 60000000 < Time.Current())
	{
		if(Server == 0) return;

		debug_printf("30秒无法联系，服务端可能已经掉线，重启Join任务，关闭Ping任务\r\n");

		Task* task = Scheduler[_TaskID];
		task->Period = 5000000;

		Server		= 0;
		Password	= 0;

		return;
	}

	TinyMessage msg;
	msg.Code = 2;

	// 没事的时候，心跳指令承载0x01子功能码，作为数据上报
	Report(msg);

	Send(msg);

	if(LastActive == 0) LastActive = Time.Current();
}

bool TinyClient::OnPing(TinyMessage& msg)
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
