#include "Time.h"

#include "Net\Net.h"
#include "TokenClient.h"

#include "TokenMessage.h"
#include "HelloMessage.h"
#include "LoginMessage.h"

bool OnTokenClientReceived(Message& msg, void* param);

void LoopTask(void* param);

static uint _taskHello = 0;

TokenClient::TokenClient() : ID(16), Key(8)
{
	Token		= 0;
	// 直接拷贝芯片ID和类型作为唯一ID
	ID.Set(Sys.ID, ID.Length());
	Key.Set(Sys.ID, Key.Length());

	Status		= 0;
	LoginTime	= 0;
	LastActive	= 0;
	Delay		= 0;

	Control		= NULL;
	//Udp			= NULL;

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

	// 设置握手广播的本地地址和端口
	ITransport* port = Control->Port;
	// C++的多接口跟C#不一样，不能简单转换了事，还需要注意两个接口的先后顺序，让它偏移
	ISocket* sock = (ISocket*)(port + 1);
	Hello.EndPoint = sock->Local;
	/*if(Udp)
	{
		Hello.EndPoint.Address	= Udp->Tip->IP;
		Hello.EndPoint.Port		= Udp->BindPort;
	}*/

	// 令牌客户端定时任务
	_taskHello = Sys.AddTask(LoopTask, this, 1000000, 5000000, "令牌客户端");
}

void TokenClient::Close()
{
	if(_taskHello) Sys.RemoveTask(_taskHello);
}

bool TokenClient::Send(TokenMessage& msg)
{
	return Control->Send(msg);
}

bool TokenClient::Reply(TokenMessage& msg)
{
	return Control->Reply(msg);
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
	//	client->SayHello(true, 3355);

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
// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + N支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + 1加密算法 + N密钥
void TokenClient::SayHello(bool broadcast, int port)
{
	TokenMessage msg(1);

	HelloMessage ext(Hello);
	ext.Reply = false;
	ext.LocalTime = Time.Current();
	ext.WriteMessage(msg);
	ext.Show(true);

	// 广播消息直接用UDP发出
	/*if(broadcast)
	{
		if(!Udp) return;

		// 临时修改远程地址为广播地址
		IPEndPoint ep = Udp->Remote;
		Udp->Remote.Address	= IPAddress::Broadcast;
		Udp->Remote.Port	= port;

		debug_printf("握手广播 ");
		Control->Send(msg);

		// 还原回来原来的地址端口
		Udp->Remote = ep;

		return;
	}*/

	Send(msg);
}

// 握手响应
bool TokenClient::OnHello(TokenMessage& msg)
{
	// 解析数据
	HelloMessage ext;
	ext.Reply = msg.Reply;
	ext.ReadMessage(msg);
	ext.Show(true);

	// 如果收到响应，并且来自来源服务器
	if(msg.Reply /*&& (Udp == NULL || Udp->CurRemote == Udp->Remote || Udp->Remote.Address.IsBroadcast())*/)
	{
		if(!msg.Error)
		{
			debug_printf("握手完成，开始登录……\r\n");
			Status = 1;

			// 通讯密码
			if(ext.Key.Length() > 0)
			{
				Control->Key = ext.Key;
				debug_printf("握手得到通信密码：");
				ext.Key.Show(true);
			}

			Login();
		}
		else
		{
			Status	= 0;
			Token	= 0;
			Stream ms(msg.Data, msg.Length);
			debug_printf("握手失败，错误码=0x%02X ", ms.Read<byte>());
			ms.ReadString().Show(true);
		}
	}
	else if(!msg.Reply)
	{
		TokenMessage rs;
		rs.Code = msg.Code;

		HelloMessage ext2(Hello);
		ext2.Reply = msg.Reply;
		ext2.LocalTime = ext.LocalTime;
		ext2.WriteMessage(rs);

		Reply(rs);
	}

	return true;
}

// 登录
void TokenClient::Login()
{
	LoginMessage login;
	login.HardID	= ID;
	login.Key		= Key;

	/*if(Udp)
	{
		login.Local.Address = Udp->Tip->IP;
		login.Local.Port	= Udp->BindPort;
	}*/

	TokenMessage msg(2);
	login.WriteMessage(msg);

	Send(msg);
}

bool TokenClient::OnLogin(TokenMessage& msg)
{
	Stream ms(msg.Data, msg.Length);

	//新令牌已经没有状态字节
	// byte result = ms.Read<byte>();

	if(!msg.Error)
	{
		Status = 2;
		debug_printf("登录成功！ ");

		// 得到令牌
		Token = ms.Read<int>();
		debug_printf("令牌：0x%08X ", Token);
		// 这里可能有通信秘密
		if(ms.Remain() > 0)
		{
			ByteArray bs = ms.ReadArray();
			if(bs.Length() > 0)
			{
				Control->Key = bs;
				debug_printf("通信密码：");
				bs.Show();
			}
		}
		debug_printf("\r\n");
	}
	else
	{
		// 登录失败，清空令牌
		Token = 0;

		byte result = ms.Read<byte>();

		if(result == 0xFF) Status = 0;

		debug_printf("登录失败，错误码 0x%02X！", result);
		ms.ReadString().Show(true);
	}

	return true;
}

// Ping指令用于保持与对方的活动状态
void TokenClient::Ping()
{
	if(LastActive > 0 && LastActive + 30000000 < Time.Current())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Hello任务
		debug_printf("30秒无法联系，服务端可能已经掉线，重新开始握手\r\n");

		Status = 0;

		return;
	}

	TokenMessage msg(3);

	Stream ms(msg.Data, ArrayLength(msg._Data));
	ms.Write((byte)8);
	ms.Write(Time.Current());
	msg.Length = ms.Position();

	Send(msg);
}

bool TokenClient::OnPing(TokenMessage& msg)
{
	// 忽略
	if(!msg.Reply) return Reply(msg);

	//debug_printf("Message_Ping Length=%d\r\n", msg.Length);

	Stream ms(msg.Data, msg.Length);

	ulong start = ms.ReadArray().ToUInt64();
	int cost = (int)(Time.Current() - start);
	if(cost < 0) cost = -cost;
	if(Delay)
		Delay = (Delay + cost) / 2;
	else
		Delay = cost;

	debug_printf("延迟 %dus / %dus \r\n", cost, Delay);

	return true;
}
