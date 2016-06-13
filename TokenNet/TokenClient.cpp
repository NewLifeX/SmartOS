#include "Time.h"

#include "Net\Socket.h"

#include "Message\BinaryPair.h"

#include "TokenClient.h"

#include "TokenMessage.h"
#include "HelloMessage.h"
#include "LoginMessage.h"
#include "RegisterMessage.h"
#include "TokenDataMessage.h"
#include "ErrorMessage.h"

#include "Security\RC4.h"

static bool OnTokenClientReceived(void* sender, Message& msg, void* param);

static void LoopTask(void* param);
static void BroadcastHelloTask(void* param);

TokenClient::TokenClient()
	: Routes(String::Compare)
{
	Token		= 0;

	Status		= 0;
	LoginTime	= 0;
	LastActive	= 0;
	Delay		= 0;

	Control		= nullptr;
	Cfg			= nullptr;

	Received	= nullptr;
	Param		= nullptr;

	Local		= nullptr;
}

void TokenClient::Open()
{
	TS("TokenClient::Open");
	assert(Control, "令牌客户端还没设置控制器呢");

	Control->Received	= OnTokenClientReceived;
	Control->Param		= this;
	Control->Open();

	//auto ctrl	= Control;
	if(Local && Local != Control)
	{
		// 向服务端握手时，汇报内网本地端口，用户端将会通过该端口连接
		//ctrl	= Local;

		Local->Received	= OnTokenClientReceived;
		Local->Param	= this;
		Local->Open();
	}

	/*// 设置握手广播的本地地址和端口
	auto sock	= ctrl->Socket;
	if(sock)
	{
		auto& ep = Hello.EndPoint;
		ep.Address	= sock->Host->IP;
		ep.Port		= sock->Local.Port;
	}
	Hello.Cipher	= "RC4";
	Hello.Name		= Cfg->User();*/

	// 令牌客户端定时任务
	_task = Sys.AddTask(LoopTask, this, 1000, 5000, "令牌客户");
	Sys.AddTask(BroadcastHelloTask, this, 2000, 30000, "令牌广播");
}

void TokenClient::Close()
{
	Sys.RemoveTask(_task);

	Control->Close();
	if(Local && Local != Control) Local->Close();
}

bool TokenClient::Send(TokenMessage& msg, TokenController* ctrl)
{
	// 未登录之前，只能 握手、登录、注册
	if(Token == 0)
	{
		if(msg.Code != 0x01 && msg.Code != 0x02 && msg.Code != 0x07) return false;
	}

	if(!ctrl) ctrl	= Control;
	assert(ctrl, "TokenClient::Send");

	return ctrl->Send(msg);
}

bool TokenClient::Reply(TokenMessage& msg, TokenController* ctrl)
{
	// 未登录之前，只能 握手、登录、注册
	if(ctrl->Token == 0)
	{
		if(msg.Code != 0x01 && msg.Code != 0x02 && msg.Code != 0x07) return false;
	}

	if(!ctrl) ctrl	= Control;
	assert(ctrl, "TokenClient::Reply");

	return ctrl->Reply(msg);
}

bool TokenClient::OnReceive(TokenMessage& msg, TokenController* ctrl)
{
	TS("TokenClient::OnReceive");

	LastActive = Sys.Ms();

	switch(msg.Code)
	{
		case 0x01:
			if(msg.Reply)
				OnHello(msg, ctrl);
			else
				OnLocalHello(msg, ctrl);
			break;
		case 0x02:
			if(msg.Reply)
				OnLogin(msg, ctrl);
			else
				OnLocalLogin(msg, ctrl);
			break;
		case 0x03:
			OnPing(msg, ctrl);
			break;
		case 0x07:
			OnRegister(msg, ctrl);
			break;
		case 0x08:
			OnInvoke(msg, ctrl);
			break;
	}
	// todo  握手登录心跳消息不需要转发
	if(msg.Code < 0x03) return true;

	// 消息转发
	if (Received)
	{
		Received(ctrl, msg, Param);
	}
	else
	{
		switch (msg.Code)
		{
		case 0x05: OnRead(msg, ctrl); break;
		case 0x06: OnWrite(msg, ctrl); break;
		default:
			break;
		}
	}

	return true;
}

bool OnTokenClientReceived(void* sender, Message& msg, void* param)
{
	auto ctrl = (TokenController*)sender;
	assert_ptr(ctrl);
	auto client = (TokenClient*)param;
	assert_ptr(client);

	return client->OnReceive((TokenMessage&)msg, ctrl);
}

// 常用系统级消息

// 定时任务
void LoopTask(void* param)
{
	TS("TokenClient::LoopTask");
	assert_ptr(param);

	auto client = (TokenClient*)param;
	//client->SayHello(true);
	// 状态。0准备、1握手完成、2登录后
	switch(client->Status)
	{
		case 0:
			client->SayHello(false);
			break;
		case 1:
		{
			if(!client->Cfg->User())
				client->Register();
			else
				client->Login();

			break;
		}
		case 2:
			client->Ping();
			break;
	}
}

void BroadcastHelloTask(void* param)
{
	TS("TokenClient::BroadcastHello");
	assert_ptr(param);

	auto client = (TokenClient*)param;
	client->SayHello(true);
}

// 发送发现消息，告诉大家我在这
// 请求：2版本 + S类型 + S名称 + 8本地时间 + 6本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8本地时间 + 6对方IP端口 + 1加密算法 + N密钥
// 错误：0xFE + 1协议 + S服务器 + 2端口
void TokenClient::SayHello(bool broadcast)
{
	TS("TokenClient::SayHello");

	auto ctrl	= broadcast ? Local : Control;
	if(!ctrl) return;

	TokenMessage msg(0x01);

	HelloMessage ext;
	ext.Reply		= false;
	ext.LocalTime	= DateTime::Now().TotalMs();

	auto sock	= Local->Socket;
	ext.EndPoint.Address	= sock->Host->IP;
	ext.EndPoint.Port		= sock->Local.Port;

	ext.Cipher	= "RC4";
	ext.Name		= Cfg->User();
	// 未注册时采用系统名称
	if(!ext.Name)
	{
		ext.Name	= Sys.Name;
		ext.Key		= Buffer(Sys.ID, 16);
	}

	ext.WriteMessage(msg);
	ext.Show(true);

	// 特殊处理广播，指定广播地址，避免因为内网发现改变了本地端口
	if(broadcast)
	{
		msg.OneWay	= true;

		msg.State	= &ctrl->Socket->Remote;
	}

	Send(msg, ctrl);
}

// 握手响应
bool TokenClient::OnHello(TokenMessage& msg, TokenController* ctrl)
{
	if(!msg.Reply) return false;

	// 解析数据
	HelloMessage ext;
	ext.Reply = msg.Reply;

	ext.ReadMessage(msg);
	ext.Show(true);

	// 如果收到响应，并且来自来源服务器
	if(msg.Error)
	{
		if(OnRedirect(ext)) return false;

		TS("TokenClient::OnHello_Error");

		Status	= 0;
		Token	= 0;

		//ext.Show(true);

		// 未握手错误，马上重新握手
		if(ext.ErrCode == 0x7F) Sys.SetTask(_task, true, 0);
	}
	else
	{
		TS("TokenClient::OnHello_Reply");

		// 通讯密码
		if(ext.Key.Length() > 0)
		{
			if(ctrl) ctrl->Key.Copy(0, ext.Key, 0, ext.Key.Length());

			debug_printf("握手得到通信密码：");
			ext.Key.Show(true);
		}
		Status = 1;

		// 握手完成后马上注册或登录
		Sys.SetTask(_task, true, 0);

		// 同步本地时间
		if(ext.LocalTime > 0) ((TTime&)Time).SetTime(ext.LocalTime / 1000);
	}

	return true;
}

bool TokenClient::OnLocalHello(TokenMessage& msg, TokenController* ctrl)
{
	if(msg.Reply) return false;

	// 解析数据
	HelloMessage ext;
	ext.Reply = msg.Reply;

	ext.ReadMessage(msg);
	ext.Show(true);

	TS("TokenClient::OnLocalHello");

	auto rs		= msg.CreateReply();

	HelloMessage ext2;
	ext2.Reply	= true;
	ext2.Key	= ctrl->Key;

	auto sock	= Local->Socket;
	ext2.EndPoint.Address	= sock->Host->IP;
	ext2.EndPoint.Port		= sock->Local.Port;

	ext2.Cipher	= "RC4";
	ext2.Name		= Cfg->User();
	// 未注册时采用系统名称
	if(!ext2.Name)
	{
		ext2.Name	= Sys.Name;
		ext2.Key	= Buffer(Sys.ID, 16);
	}

	// 使用当前时间
	ext2.LocalTime = DateTime::Now().TotalMs();
	ext2.WriteMessage(rs);

	Reply(rs, ctrl);

	return true;
}

bool TokenClient::OnRedirect(HelloMessage& msg)
{
	TS("TokenClient::OnRedirect");

	if(!(msg.ErrCode == 0xFE || msg.ErrCode == 0xFD)) return false;

	auto cfg		= Cfg;
	cfg->Protocol	= (ProtocolType)msg.Protocol;

	cfg->Show();

	cfg->Server()	= msg.Server;
	cfg->ServerPort = msg.Port;
	cfg->Token()	= msg.VisitToken;

	ChangeIPEndPoint(msg.Server, msg.Port);

	cfg->Show();

	// 0xFE永久改变厂商地址
	if(msg.ErrCode == 0xFE) cfg->Save();

	// 马上开始重新握手
	Status = 0;
	Sys.SetTask(_task, true, 0);

	return true;
}

bool TokenClient::ChangeIPEndPoint(const String& domain, ushort port)
{
	TS("TokenClient::ChangeIPEndPoint");

	debug_printf("ChangeIPEndPoint ");

	domain.Show(true);

    auto socket = Control->Socket;
	if(socket == nullptr) return false;

	Control->Port->Close();
	//socket->Remote.Address	= ip;
	socket->Remote.Port	= port;
	socket->Server		= domain;
	//socket->Change(domain, port);

	// 等下次发数据的时候自动打开
	//Control->Port->Open();

	Cfg->ServerIP	= socket->Remote.Address.Value;

	return true;
}

// 注册
void TokenClient::Register()
{
	TS("TokenClient::Register");

	debug_printf("TokenClient::Register\r\n");

	RegisterMessage reg;
	reg.User	= Buffer(Sys.ID, 16).ToHex();

	// 原始密码作为注册密码
	reg.Pass	= Cfg->Pass();

	auto now	= Sys.Ms();
	reg.Salt	= Buffer(&now, 8);

	reg.Show(true);

	TokenMessage msg(7);
	reg.WriteMessage(msg);
	Send(msg);
}

void TokenClient::OnRegister(TokenMessage& msg, TokenController* ctrl)
{
	if(!msg.Reply) return;

	TS("TokenClient::OnRegister");

	if(msg.Error)
	{
		//auto ms		= msg.ToStream();
		//byte result = ms.ReadByte();
		ErrorMessage em;
		em.Read(msg);

		debug_printf("注册失败，错误码 0x%02X！", em.ErrorCode);
		//ms.ReadString().Show(true);
		em.ErrorContent.Show(true);

		return;
	}

	auto cfg	= Cfg;

	RegisterMessage reg;
	reg.ReadMessage(msg);
	cfg->User()	= reg.User;
	cfg->Pass()	= reg.Pass;

	cfg->Show();
	cfg->Save();

	//Hello.Name	= reg.User;

	Status	= 0;

	Sys.SetTask(_task, true, 0);
}

// 登录
void TokenClient::Login()
{
	TS("TokenClient::Login");

	LoginMessage login;

	auto cfg	= Cfg;
	login.User	= cfg->User();
	//login.Pass	= MD5::Hash(cfg->Pass);

	// 原始密码对盐值进行加密，得到登录密码
	auto now	= Sys.Ms();
	auto arr	= Buffer(&now, 8);
	login.Salt	= arr;
	RC4::Encrypt(arr, cfg->Pass());
	login.Pass	= arr.ToHex();

	TokenMessage msg(2);
	login.WriteMessage(msg);
	login.Show(true);

	Send(msg);
}

bool TokenClient::OnLogin(TokenMessage& msg, TokenController* ctrl)
{
	if(!msg.Reply) return false;

	TS("TokenClient::OnLogin");

	Stream ms(msg.Data, msg.Length);

	if(msg.Error)
	{
		// 登录失败，清空令牌
		Token = 0;

		byte result = ms.ReadByte();
		// 任何错误，重新握手
		Status = 0;

		debug_printf("登录失败，错误码 0x%02X！", result);
		ms.ReadString().Show(true);

		// 未登录错误，马上重新登录
		if(result == 0x7F) Sys.SetTask(_task, true, 0);
	}
	else
	{
		Status = 2;
		debug_printf("登录成功！ ");

		LoginMessage logMsg;
		logMsg.ReadMessage(msg);
		Token = logMsg.Token;

		if(ctrl) ctrl->Token = Token;
		logMsg.Show(true);
		debug_printf("令牌：0x%08X ", Token);
		if (logMsg.Key.Length())
		{
			if(ctrl) ctrl->Key = logMsg.Key;

			debug_printf("通信密码：");
			logMsg.Key.Show();
		}

		debug_printf("\r\n");

		// 登录成功后加大心跳间隔
		Sys.SetTaskPeriod(_task, 30000);
	}

	return true;
}

bool TokenClient::OnLocalLogin(TokenMessage& msg, TokenController* ctrl)
{
	if(msg.Reply) return false;

	TS("TokenClient::OnLocalLogin");

	auto rs	= msg.CreateReply();

	LoginMessage login;
	// 这里需要随机密匙
	login.Key		= ctrl->Key;
	// 随机令牌
	login.Token		= Sys.Ms();
	login.Reply		= true;
	login.WriteMessage(rs);

	Reply(rs, ctrl);


	//ctrl2->Key.Copy(0, login.User, 0, -1);
	ctrl->Token 	= login.Token;

	return true;
}

// Ping指令用于保持与对方的活动状态
void TokenClient::Ping()
{
	TS("TokenClient::Ping");

	if(LastActive > 0 && LastActive + 180000 < Sys.Ms())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Hello任务
		debug_printf("180秒无法联系，服务端可能已经掉线，重新开始握手\r\n");

		Control->Key.SetLength(0);

		Status = 0;

		Sys.SetTaskPeriod(_task, 5000);

		return;
	}

	TokenPingMessage pinMsg;

	TokenMessage msg(3);
	pinMsg.WriteMessage(msg);

	Send(msg);
}

bool TokenClient::OnPing(TokenMessage& msg, TokenController* ctrl)
{
	TS("TokenClient::OnPing");

	if (!msg.Reply) return false;

	TokenPingMessage pinMsg;
	pinMsg.ReadMessage(msg);
	UInt64 start = pinMsg.LocalTime;

	UInt64 now = DateTime::Now().TotalMs();
	int cost 	= (int)(now - start);
	//if(cost < 0) cost = -cost;
	// if(cost > 1000) ((TTime&)Time).SetTime(start / 1000);

	if(Delay)
		Delay = (Delay + cost) / 2;
	else
		Delay = cost;

	debug_printf("心跳延迟 %dms / %dms \r\n", cost, Delay);
	return true;
}

/******************************** 数据区 ********************************/

void TokenClient::Read(int start, int size)
{
	TokenDataMessage dm;
	dm.Start	= start;
	dm.Size		= size;

	TokenMessage msg;
	msg.Code	= 0x05;
	dm.WriteMessage(msg);

	Control->Send(msg);
}

void TokenClient::Write(int start, const Buffer& bs)
{
	TokenDataMessage dm;
	dm.Start	= start;
	dm.Data		= bs;

	TokenMessage msg;
	msg.Code	= 0x06;
	dm.WriteMessage(msg);

	Control->Send(msg);
}

/*
请求：起始 + 大小
响应：起始 + 数据
*/
void TokenClient::OnRead(const TokenMessage& msg, TokenController* ctrl)
{
	if(msg.Reply) return;
	if(msg.Length < 2) return;

	auto rs	= msg.CreateReply();
	auto ms	= rs.ToStream();

	TokenDataMessage dm;
	dm.ReadMessage(msg);
	dm.Show(true);

	bool rt	= true;
	if(dm.Start < 64)
		rt	= dm.ReadData(Store);
	else if(dm.Start < 128)
	{
		dm.Start	-= 64;
		rt	= dm.ReadData(Cfg->ToArray());
		dm.Start	+= 64;
	}

	if (!rt)
	{
		debug_printf("rt == false\r\n");
		rs.Error = true;
	}
	else
	{
		dm.Show(true);
		dm.WriteMessage(rs);
	}

	Reply(rs, ctrl);
}

/*
请求：1起始 + N数据
响应：1起始 + 1大小 + N数据
错误：错误码2 + 1起始 + 1大小
*/
void TokenClient::OnWrite(const TokenMessage& msg, TokenController* ctrl)
{
	if(msg.Reply) return;
	if(msg.Length < 2) return;

	auto rs	= msg.CreateReply();
	auto ms	= rs.ToStream();

	TokenDataMessage dm;
	dm.ReadMessage(msg);
	dm.Show(true);

	bool rt	= true;
	if(dm.Start < 64)
	{
		rt	= dm.WriteData(Store, true);
	}
	else if(dm.Start < 128)
	{
		dm.Start	-= 64;
		auto bs	= Cfg->ToArray();
		rt	= dm.WriteData(bs, true);
		dm.Start	+= 64;

		Cfg->Save();
	}

	if(!rt)
		rs.Error	= true;
	else
		dm.WriteMessage(rs);

	Reply(rs, ctrl);

	if(dm.Start >= 64 && dm.Start < 128)
	{
		debug_printf("\r\n 配置区被修改，200ms后重启\r\n");
		Sys.Sleep(200);
		Sys.Reset();
	}
}

/******************************** 远程调用 ********************************/

void TokenClient::Invoke(const String& action, const Buffer& bs)
{
	TokenMessage msg;
	auto ms	= msg.ToStream();

	BinaryPair bp(ms);

	bp.Set("Action", action);

	ms.Write(bs);

	Control->Send(msg);
}

void TokenClient::OnInvoke(const TokenMessage& msg, TokenController* ctrl)
{
	auto rs	= msg.CreateReply();
	auto ms	= rs.ToStream();
	// 考虑到结果可能比较大，允许扩容
	ms.CanResize	= true;

	BinaryPair bp(msg.ToStream());

	String action;
	if(!bp.Get("Action", action) || !action)
	{
		rs.SetError(0x01, "请求错误");
	}
	else
	{
		void* handler	= nullptr;
		if(!Routes.TryGetValue(action.GetBuffer(), handler) || !handler)
		{
			rs.SetError(0x02, "操作注册有误");
		}
		else
		{
			auto ivh	= (InvokeHandler)handler;

			BinaryPair result(ms);
			if(!ivh(bp, result))
			{
				rs.SetError(0x03, "执行出错");
			}
			else
			{
				// 执行成功
				// 数据流可能已经扩容
				rs.Data		= ms.GetBuffer();
				rs.Length	= ms.Position();
			}
		}
	}

	ctrl->Reply(rs);
}
