﻿#include "TokenClient.h"
#include "SerialPort.h"

#include "TokenMessage.h"
#include "TokenNet\HelloMessage.h"

#include "Security\MD5.h"

bool OnTokenClientReceived(Message& msg, void* param);

void LoopTask(void* param);

static uint _taskHello = 0;

TokenClient::TokenClient() : ID(16), Key(8)
{
	Token		= 0;
	//memset(ID, 0, ArrayLength(ID));
	//memset(Key, 0, ArrayLength(Key));
	// 直接拷贝芯片ID和类型作为唯一ID
	ID.Set(Sys.ID, ID.Length());
	Key.Set(Sys.ID, Key.Length());

	Status		= 0;
	LoginTime	= 0;
	LastActive	= 0;

	Control		= NULL;
	Udp			= NULL;

	Received	= NULL;
	Param		= NULL;

	Switchs		= NULL;
	Regs		= NULL;
}

void TokenClient::Open()
{
	assert_param2(Control, "令牌客户端还没设置控制器呢");

	Control->Open();

	Control->Received	= OnTokenClientReceived;
	Control->Param		= this;

#if DEBUG
	//Control->AddTransport(SerialPort::GetMessagePort());
#endif

	// 注册消息。每个消息代码对应一个功能函数
	//Control->Register(1, OnHello, this);
	//Control->Register(2, Ping, this);
	//Control->Register(3, SysID, this);
	//Control->Register(4, SysTime, this);
	//Control->Register(5, SysMode, this);

	// 设置握手广播的本地地址和端口
	if(Udp)
	{
		Hello.EndPoint.Address	= Udp->Tip->IP;
		Hello.EndPoint.Port		= Udp->BindPort;
	}

	// 令牌客户端定时任务
	debug_printf("Token::Open 令牌客户端定时 ");
	_taskHello = Sys.AddTask(LoopTask, this, 1000000, 5000000);
}

void TokenClient::Close()
{
	if(_taskHello) Sys.RemoveTask(_taskHello);
}

void TokenClient::Send(TokenMessage& msg)
{
	Control->Send(msg);
}

void TokenClient::Reply(TokenMessage& msg)
{
	Control->Reply(msg);
}

bool TokenClient::OnReceive(TokenMessage& msg)
{
	LastActive = Time.Current();

	switch(msg.Code)
	{
		case 1:
			OnHello(msg);
			break;
		case 2:
			OnLogin(msg);
			break;
		case 3:
			OnPing(msg);
			break;
	}

	// 消息转发
	if(Received) Received(msg, Param);

	return true;
}

bool OnTokenClientReceived(Message& msg, void* param)
{
	TokenClient* client = (TokenClient*)param;
	assert_ptr(client);

	return client->OnReceive((TokenMessage&)msg);
}

// 常用系统级消息

// 定时任务
void LoopTask(void* param)
{
	assert_ptr(param);
	TokenClient* client = (TokenClient*)param;
	//client->SayHello(false);
	//if(client->Udp->BindPort != 3355)
	client->SayHello(true, 3355);

	// 状态。0准备、1握手完成、2登录后
	switch(client->Status)
	{
		case 0:
			client->SayHello(false);
			break;
		case 1:
			client->Login();
			break;
		case 2:
			client->Ping();
			break;
	}
}

// 发送发现消息，告诉大家我在这
// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥
void TokenClient::SayHello(bool broadcast, int port)
{
	TokenMessage msg(1);

	HelloMessage ext(Hello);
	ext.LocalTime = Time.Current();
	ext.Write(msg);
	ext.Show();

	// 广播消息直接用UDP发出
	if(broadcast)
	{
		if(!Udp) return;

		// 临时修改远程地址为广播地址
		IPEndPoint ep = Udp->Remote;
		Udp->Remote.Address	= IPAddress::Broadcast;
		Udp->Remote.Port	= port;

		Control->Send(msg, Udp);

		// 还原回来原来的地址端口
		Udp->Remote = ep;

		return;
	}

	bool rs = Control->SendAndReceive(msg, 0, 200);

	// 如果收到响应，解析出来
	if(rs && msg.Reply)
	{
		debug_printf("握手完成，开始登录……\r\n");
		Status = 1;

		ext.Read(msg);
		ext.Show();

		Login();
	}
}

// 握手响应
bool TokenClient::OnHello(TokenMessage& msg)
{
	// 解析数据
	HelloMessage ext;
	ext.Read(msg);
	ext.Show();

	// 取消Discover任务
	//debug_printf("停止寻找服务端 ");
	//Sys.RemoveTask(_taskHello);
	//_taskHello = 0;

	return true;
}

// 登录
void TokenClient::Login()
{
	TokenMessage msg(2);

	Stream ms(msg.Data, ArrayLength(msg._Data));
	ms.WriteArray(ID);

	// 密码取MD5后传输
	ByteArray bs(16);
	MD5::Hash(Key, bs);
	ms.WriteArray(bs);

	ms.Write(Time.Current());

	msg.Length = ms.Position();

	Send(msg);
}

bool TokenClient::OnLogin(TokenMessage& msg)
{
	Stream ms(msg.Data, msg.Length);
	byte result = ms.Read<byte>();

	if(!result)
	{
		Status = 2;
		debug_printf("登录成功！\r\n");
	}
	else
	{
		debug_printf("登录失败，错误码 0x%02X！", result);
		String str;
		ms.ReadString(str.Clear());
		str.Show();
		debug_printf("\r\n");
	}

	return true;
}

// Ping指令用于保持与对方的活动状态
void TokenClient::Ping()
{
	if(LastActive > 0 && LastActive + 30000000 < Time.Current())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Discover任务，关闭Ping任务
		debug_printf("30秒无法联系，服务端可能已经掉线，重新开始握手\r\n");

		Status = 0;

		return;
	}

	TokenMessage msg(3);

	Send(msg);
}

bool TokenClient::OnPing(TokenMessage& msg)
{
	// 忽略响应消息
	if(msg.Reply) return true;

	debug_printf("Message_Ping Length=%d\r\n", msg.Length);

	return true;
}

/*// 系统时间获取与设置
bool TokenClient::SysTime(TokenMessage& msg, void* param)
{
	TokenMessage& tmsg = (TokenMessage&)msg;
	// 忽略响应消息
	if(tmsg.Reply) return true;

	debug_printf("Message_SysTime Length=%d\r\n", msg.Length);

	// 负载数据决定是读时间还是写时间
	if(msg.Length >= 8)
	{
		// 写时间
		ulong us = *(ulong*)msg.Data;

		Time.SetTime(us);
	}

	// 读时间
	ulong us2 = Time.Current();
	msg.Length = 8;
	*(ulong*)msg.Data = us2;

	return true;
}

bool TokenClient::SysID(TokenMessage& msg, void* param)
{
	TokenMessage& tmsg = (TokenMessage&)msg;
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

bool TokenClient::SysMode(TokenMessage& msg, void* param)
{
	TokenMessage& tmsg = (TokenMessage&)msg;
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
