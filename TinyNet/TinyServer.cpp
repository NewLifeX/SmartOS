#include "TinyServer.h"
#include "SerialPort.h"

#include "TinyMessage.h"
#include "TokenMessage.h"

bool OnServerReceived(Message& msg, void* param);

void DiscoverClientTask(void* param);
void PingClientTask(void* param);

static uint _taskDiscover = 0;
static uint _taskPing = 0;

TinyServer::TinyServer(Controller* control)
{
	_control = control;

	DeviceType	= 0;
	Password	= 0;

	LastActive	= 0;

	_control->Received	= OnServerReceived;
	_control->Param		= this;

	Received	= NULL;
	Param		= NULL;

	OnDiscover	= NULL;
	OnPing		= NULL;
}

void TinyServer::Send(Message& msg)
{
	_control->Send(msg);
}

bool OnServerReceived(Message& msg, void* param)
{
	TinyServer* server = (TinyServer*)param;
	assert_ptr(server);
	
	// 消息转发
	if(server->Received) return server->Received(msg, server->Param);

	return true;
}

// 常用系统级消息

void TinyServer::Start()
{
#if DEBUG
	_control->AddTransport(SerialPort::GetMessagePort());
#endif

	// 注册消息。每个消息代码对应一个功能函数
	_control->Register(1, Discover, this);
	_control->Register(2, Ping, this);
	_control->Register(3, SysID, this);
	_control->Register(4, SysTime, this);
	_control->Register(5, SysMode, this);

	// 发现服务端的任务
	debug_printf("开始寻找服务端 ");
	_taskDiscover = Sys.AddTask(DiscoverClientTask, this, 0, 2000000);
}

// 最后发送Discover消息的ID，防止被别人欺骗，直接向我发送Discover响应
static byte _lastDiscoverID;

void DiscoverClientTask(void* param)
{
	assert_ptr(param);
	TinyServer* client = (TinyServer*)param;
	client->Discover();
}

// 发送发现消息，告诉大家我在这
// 格式：2字节设备类型 + 20字节系统ID
void TinyServer::Discover()
{
	TinyMessage msg;
	msg.Code = 1;

	// 发送的广播消息，设备类型和系统ID
	Stream ms(msg._Data, ArrayLength(msg._Data));
	ms.Length = 0;
	ms.Write(DeviceType);
	ms.Write(Sys.ID, 0, 20);
	msg.Length = ms.Length;

	_control->Send(msg);

	_lastDiscoverID = msg.Sequence;
}

// Discover响应
// 格式：1字节地址 + 8字节密码
bool TinyServer::Discover(Message& msg, void* param)
{
	TinyMessage& tmsg = (TinyMessage&)msg;
	// 客户端只处理Discover响应
	if(!tmsg.Reply || tmsg.Error) return true;

	// 校验不对
	if(_lastDiscoverID != tmsg.Sequence) return true;

	assert_ptr(param);
	TinyServer* client = (TinyServer*)param;

	// 解析数据
	Stream ms(msg.Data, msg.Length);
	TinyController* ctrl = (TinyController*)client->_control;
	if(ms.Remain())
		ctrl->Address = ms.Read<byte>();
	if(ms.Remain() >= 8)
		client->Password = ms.Read<ulong>();

	// 记住服务端地址
	//client->Server = tmsg.Src;

	// 取消Discover任务
	debug_printf("停止寻找服务端 ");
	Sys.RemoveTask(_taskDiscover);
	_taskDiscover = 0;

	// 启动Ping任务
	debug_printf("开始Ping服务端 ");
	_taskPing = Sys.AddTask(PingClientTask, client, 0, 5000000);

	if(client->OnDiscover) return client->OnDiscover(msg, param);

	return true;
}

void PingClientTask(void* param)
{
	assert_ptr(param);
	TinyServer* client = (TinyServer*)param;
	client->Ping();
}

// Ping指令用于保持与对方的活动状态
void TinyServer::Ping()
{
	if(LastActive > 0 && LastActive + 30000000 < Time.Current())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Discover任务，关闭Ping任务
		debug_printf("30秒无法联系，服务端可能已经掉线，重启Discover任务，关闭Ping任务\r\n");

		debug_printf("停止Ping服务端 ");
		Sys.RemoveTask(_taskPing);

		debug_printf("开始寻找服务端 ");
		_taskDiscover = Sys.AddTask(DiscoverClientTask, this, 0, 2000000);

		//Server = 0;
		Password = 0;

		return;
	}

	TinyMessage msg;
	msg.Code = 2;

	_control->Send(msg);

	if(LastActive == 0) LastActive = Time.Current();
}

bool TinyServer::Ping(Message& msg, void* param)
{
	assert_ptr(param);
	TinyServer* client = (TinyServer*)param;

	TinyMessage& tmsg = (TinyMessage&)msg;
	// 忽略响应消息
	if(tmsg.Reply)
	{
		//if(msg.Src == client->Server) client->LastActive = Time.Current();
		return true;
	}

	debug_printf("Message_Ping Length=%d\r\n", msg.Length);

	if(client->OnPing) return client->OnPing(msg, param);

	return true;
}

// 系统时间获取与设置
bool TinyServer::SysTime(Message& msg, void* param)
{
	TinyMessage& tmsg = (TinyMessage&)msg;
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

bool TinyServer::SysID(Message& msg, void* param)
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

bool TinyServer::SysMode(Message& msg, void* param)
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
}
