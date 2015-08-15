#include "Time.h"
#include "TinyClient.h"

#include "TinyMessage.h"
#include "TokenMessage.h"

#include "TokenNet\DiscoverMessage.h"

bool OnClientReceived(Message& msg, void* param);

void DiscoverTask(void* param);
void PingTask(void* param);

static uint _taskDiscover = 0;
static uint _taskPing = 0;

TinyClient::TinyClient(TinyController* control)
{
	assert_ptr(control);

	_control = control;

	Server		= 0;
	Type		= Sys.Code;
	Password	= 0;

	LastActive	= 0;

	_control->Received	= OnClientReceived;
	_control->Param		= this;

	Received	= NULL;
	Param		= NULL;

	OnDiscover	= NULL;
	OnPing		= NULL;

	Switchs		= 0;
	Analogs		= 0;
}

void TinyClient::Send(TinyMessage& msg)
{
	assert_param2(this, "令牌客户端未初始化");
	assert_param2(_control, "令牌控制器未初始化");

	// 设置网关地址
	if(!msg.Dest) msg.Dest = Server;

	_control->Send(msg);
}

void TinyClient::Reply(TinyMessage& msg)
{
	assert_param2(this, "令牌客户端未初始化");
	assert_param2(_control, "令牌控制器未初始化");

	_control->Reply(msg);
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

	// 消息转发
	if(Received) return Received(msg, Param);

	return true;
}

// 常用系统级消息

void TinyClient::SetDefault()
{
	// 打开传输口
	_control->Open();

	// 注册消息。每个消息代码对应一个功能函数
	_control->Register(1, Discover, this);
	_control->Register(2, Ping, this);
	_control->Register(3, SysID, this);
	_control->Register(4, SysTime, this);
	_control->Register(5, SysMode, this);

	// 发现服务端的任务
	//debug_printf("开始寻找服务端 ");
	_taskDiscover = Sys.AddTask(DiscoverTask, this, 0, 5000000, "发现服务");
}

// 最后发送Discover消息的ID，防止被别人欺骗，直接向我发送Discover响应
static byte _lastDiscoverID;

void DiscoverTask(void* param)
{
	assert_ptr(param);
	TinyClient* client = (TinyClient*)param;
	client->Discover();
}

// 发送发现消息，告诉大家我在这
// 格式：2设备类型 + N系统ID
void TinyClient::Discover()
{
	TinyMessage msg;
	msg.Code = 1;

	// 发送的广播消息，设备类型和系统ID
	DiscoverMessage dm;
	dm.Type		= Type;
	dm.HardID	= Sys.ID;
	dm.Switchs	= Switchs;
	dm.Analogs	= Analogs;
	dm.WriteMessage(msg);
	dm.Show(true);

	_control->Broadcast(msg);

	_lastDiscoverID = msg.Sequence;
}

// Discover响应
// 格式：1地址 + N密码
bool TinyClient::Discover(Message& msg, void* param)
{
	TinyMessage& tmsg = (TinyMessage&)msg;
	// 客户端只处理Discover响应
	if(!tmsg.Reply || tmsg.Error) return true;

	// 校验不对
	if(_lastDiscoverID != tmsg.Sequence)
	{
		debug_printf("发现响应序列号 %d 不等于内部序列号 %d \r\n", tmsg.Sequence, _lastDiscoverID);
		//return true;
	}

	assert_ptr(param);
	TinyClient* client = (TinyClient*)param;
	TinyController* ctrl = (TinyController*)client->_control;

	// 解析数据
	DiscoverMessage dm;
	dm.ReadMessage(msg);
	dm.Show(true);

	ctrl->Address		= dm.ID;
	client->Password	= dm.Pass;

	// 记住服务端地址
	client->Server = tmsg.Src;

	// 取消Discover任务
	if(_taskDiscover)
	{
		debug_printf("停止寻找服务端 ");
		Sys.RemoveTask(_taskDiscover);
		_taskDiscover = 0;
	}

	// 启动Ping任务
	if(!_taskPing)
	{
		//debug_printf("开始Ping服务端 ");
		_taskPing = Sys.AddTask(PingTask, client, 0, 15000000, "心跳");
	}

	if(client->OnDiscover) return client->OnDiscover(msg, param);

	return true;
}

void PingTask(void* param)
{
	assert_ptr(param);
	TinyClient* client = (TinyClient*)param;
	client->Ping();
}

// Ping指令用于保持与对方的活动状态
void TinyClient::Ping()
{
	if(LastActive > 0 && LastActive + 60000000 < Time.Current())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Discover任务，关闭Ping任务
		debug_printf("30秒无法联系，服务端可能已经掉线，重启Discover任务，关闭Ping任务\r\n");

		debug_printf("停止Ping服务端 ");
		Sys.RemoveTask(_taskPing);
		_taskPing = 0;

		debug_printf("开始寻找服务端 ");
		_taskDiscover = Sys.AddTask(DiscoverTask, this, 0, 5000000);

		Server = 0;
		Password = 0;

		return;
	}

	TinyMessage msg;
	msg.Code = 2;

	//_control->Broadcast(msg);
	Send(msg);

	if(LastActive == 0) LastActive = Time.Current();
}

bool TinyClient::Ping(Message& msg, void* param)
{
	assert_ptr(param);
	TinyClient* client = (TinyClient*)param;

	TinyMessage& tmsg = (TinyMessage&)msg;
	// 仅处理来自网关的消息
	if(client->Server == 0 || client->Server != tmsg.Dest) return true;

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
bool TinyClient::SysTime(Message& msg, void* param)
{
	TinyMessage& tmsg = (TinyMessage&)msg;
	// 忽略响应消息
	if(tmsg.Reply) return true;

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
}
