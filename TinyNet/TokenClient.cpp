#include "TokenClient.h"
#include "SerialPort.h"

#include "TokenMessage.h"
#include "TokenNet\HelloMessage.h"

bool OnTokenClientReceived(Message& msg, void* param);

void HelloTask(void* param);
void PingTask(void* param);

static uint _taskHello = 0;
//static uint _taskPing = 0;

TokenClient::TokenClient()
{
	Token		= 0;
	memset(ID, 0, ArrayLength(ID));
	memset(Key, 0, ArrayLength(Key));

	LoginTime	= 0;
	LastActive	= 0;

	Received	= NULL;
	Param		= NULL;

	OnHello	= NULL;
	//OnPing		= NULL;

	Switchs		= NULL;
	Regs		= NULL;
}

void TokenClient::Open()
{
	assert_ptr(Control);

	Control->Received	= OnTokenClientReceived;
	Control->Param		= this;

#if DEBUG
	//Control->AddTransport(SerialPort::GetMessagePort());
#endif

	// 注册消息。每个消息代码对应一个功能函数
	Control->Register(1, SayHello, this);
	//Control->Register(2, Ping, this);
	//Control->Register(3, SysID, this);
	//Control->Register(4, SysTime, this);
	//Control->Register(5, SysMode, this);

	// 发现服务端的任务
	debug_printf("TokenClient::Open 开始寻找服务端 ");
	_taskHello = Sys.AddTask(HelloTask, this, 0, 5000000);
}

void TokenClient::Send(Message& msg)
{
	Control->Send(msg);
}

void TokenClient::Reply(Message& msg)
{
	Control->Reply(msg);
}

bool OnTokenClientReceived(Message& msg, void* param)
{
	TokenClient* client = (TokenClient*)param;
	assert_ptr(client);

	// 消息转发
	if(client->Received) return client->Received(msg, client->Param);

	return true;
}

// 常用系统级消息

void HelloTask(void* param)
{
	assert_ptr(param);
	TokenClient* client = (TokenClient*)param;
	client->SayHello();
}

// 发送发现消息，告诉大家我在这
// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥
void TokenClient::SayHello()
{
	TokenMessage msg(1);

	// 发送的广播消息，设备类型和系统ID
	Stream ms(msg._Data, ArrayLength(msg._Data));
	ms.Length = 0;

	HelloMessage ext(Hello);
	ext.Write(ms);

	msg.Length = ms.Length;

	bool rs = Control->SendAndReceive(msg, 0, 200);

	// 如果收到响应，解析出来
	if(rs && msg.Reply)
	{
		msg.Show();

		Stream ms2(msg._Data, msg.Length);
		ext.Read(ms2);
		ext.Show();
	}
}

// Discover响应
// 格式：1字节地址 + 8字节密码
bool TokenClient::SayHello(Message& msg, void* param)
{
	TokenMessage& tmsg = (TokenMessage&)msg;
	// 客户端只处理Discover响应
	if(!tmsg.Reply) return true;

	assert_ptr(param);
	TokenClient* client = (TokenClient*)param;
	//TokenController* ctrl = (TokenController*)client->Control;

	// 解析数据
	Stream ms(msg.Data, msg.Length);

	// 取消Discover任务
	//debug_printf("停止寻找服务端 ");
	//Sys.RemoveTask(_taskHello);
	//_taskHello = 0;

	if(client->OnHello) return client->OnHello(msg, param);

	return true;
}

/*void PingTask(void* param)
{
	assert_ptr(param);
	TokenClient* client = (TokenClient*)param;
	client->Ping();
}

// Ping指令用于保持与对方的活动状态
void TokenClient::Ping()
{
	if(LastActive > 0 && LastActive + 30000000 < Time.Current())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Discover任务，关闭Ping任务
		debug_printf("30秒无法联系，服务端可能已经掉线，重启Discover任务，关闭Ping任务\r\n");

		debug_printf("停止Ping服务端 ");
		Sys.RemoveTask(_taskPing);

		debug_printf("开始寻找服务端 ");
		_taskHello = Sys.AddTask(DiscoverTask, this, 0, 2000000);

		Server = 0;
		Password = 0;

		return;
	}

	Message* msg = Control->Create();
	msg->Code = 2;

	Control->Send(*msg);

	delete msg;

	if(LastActive == 0) LastActive = Time.Current();
}

bool TokenClient::Ping(Message& msg, void* param)
{
	assert_ptr(param);
	TokenClient* client = (TokenClient*)param;

	TokenMessage& tmsg = (TokenMessage&)msg;
	// 忽略响应消息
	if(tmsg.Reply)
	{
		if(tmsg.Src == client->Server) client->LastActive = Time.Current();
		return true;
	}

	debug_printf("Message_Ping Length=%d\r\n", msg.Length);

	if(client->OnPing) return client->OnPing(msg, param);

	return true;
}

// 系统时间获取与设置
bool TokenClient::SysTime(Message& msg, void* param)
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

bool TokenClient::SysID(Message& msg, void* param)
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

bool TokenClient::SysMode(Message& msg, void* param)
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
