#include "Time.h"

#include "Net\Net.h"
#include "Net\DNS.h"

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

TokenClient::TokenClient()
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
	LocalAP		= nullptr;
}

void TokenClient::Open()
{
	TS("TokenClient::Open");
	assert(Control, "令牌客户端还没设置控制器呢");

	Control->Received	= OnTokenClientReceived;
	Control->Param		= this;
	Control->Open();

	auto ctrl	= Control;
	if(Local && Local != Control)
	{
		// 向服务端握手时，汇报内网本地端口，用户端将会通过该端口连接
		ctrl	= Local;

		Local->Received	= OnTokenClientReceived;
		Local->Param	= this;
		Local->Open();
	}

	// 设置握手广播的本地地址和端口
	auto sock	= dynamic_cast<ISocket*>(ctrl->Port);
	if(sock)
	{
		auto& ep = Hello.EndPoint;
		ep.Address	= sock->Host->IP;
		ep.Port		= sock->Local.Port;
	}

	// 令牌客户端定时任务
	_task = Sys.AddTask(LoopTask, this, 1000, 5000, "令牌客户端");
}

void TokenClient::Close()
{
	Sys.RemoveTask(_task);

	Control->Close();
	if(Local && Local != Control) Local->Close();
}

bool TokenClient::Send(TokenMessage& msg, Controller* ctrl)
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

bool TokenClient::Reply(TokenMessage& msg, Controller* ctrl)
{
	auto tctrl = dynamic_cast<TokenController*>(ctrl);
	// 未登录之前，只能 握手、登录、注册
	if(tctrl->Token == 0)
	{
		if(msg.Code != 0x01 && msg.Code != 0x02 && msg.Code != 0x07) return false;
	}

	if(!ctrl) ctrl	= Control;
	assert(ctrl, "TokenClient::Reply");

	return ctrl->Reply(msg);
}

bool TokenClient::OnReceive(TokenMessage& msg, Controller* ctrl)
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
	auto ctrl = (Controller*)sender;
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
		{
			if(!client->Cfg->User)
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

// 发送发现消息，告诉大家我在这
// 请求：2版本 + S类型 + S名称 + 8本地时间 + 6本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8本地时间 + 6对方IP端口 + 1加密算法 + N密钥
// 错误：0xFE + 1协议 + S服务器 + 2端口
void TokenClient::SayHello(bool broadcast, int port)
{
	TS("TokenClient::SayHello");

	TokenMessage msg(0x01);

	HelloMessage ext(Hello);
	ext.Reply		= false;
	ext.LocalTime	= Time.Now().TotalMs();

	// 设置握手广播的本地地址和端口
	//auto socket		= dynamic_cast<ISocket*>(Control->Port);
	//ext.EndPoint.Address	= socket->Host->IP;

	ext.WriteMessage(msg);
	ext.Show(true);

	Send(msg);
}

// 握手响应
bool TokenClient::OnHello(TokenMessage& msg, Controller* ctrl)
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
		debug_printf("握手失败，错误码=0x%02X ", ext.ErrCode);

		ext.ErrMsg.Show(true);

		// 未握手错误，马上重新握手
		if(ext.ErrCode == 0x7F) Sys.SetTask(_task, true, 0);
	}
	else
	{
		TS("TokenClient::OnHello_Reply");

		// 通讯密码
		if(ext.Key.Length() > 0)
		{
			auto ctrl2	= dynamic_cast<TokenController*>(ctrl);
			if(ctrl2) ctrl2->Key.Copy(0, ext.Key, 0, ext.Key.Length());

			debug_printf("握手得到通信密码：");
			ext.Key.Show(true);
		}
		Status = 1;

		// 握手完成后马上注册或登录
		Sys.SetTask(_task, true, 0);

		DateTime dt(ext.LocalTime / 1000UL);
		// 同步本地时间
		if(ext.LocalTime > 0) ((TTime&)Time).SetTime(dt.TotalSeconds());
	}

	return true;
}

bool TokenClient::OnLocalHello(TokenMessage& msg, Controller* ctrl)
{
	if(msg.Reply) return false;

	auto ctrl2 = dynamic_cast<TokenController*>(ctrl);

	// 解析数据
	HelloMessage ext;
	ext.Reply = msg.Reply;

	ext.ReadMessage(msg);
	ext.Show(true);

	TS("TokenClient::OnLocalHello");

	auto rs		= msg.CreateReply();

	HelloMessage ext2(Hello);
	ext2.Reply	= true;
	// 使用系统ID作为Name
	ext2.Name	= Cfg->User;
	// 使用系统ID作为Key
	ext2.Key	= ctrl2->Key;
	//ext2.Key	= Sys.ID;
	//auto ctrl3	= dynamic_cast<TokenController*>(ctrl);
	//if(ctrl3) ctrl3->Key = ext2.Key;

	ext2.Cipher	= "RC4";
	//ext2.LocalTime = ext.LocalTime;
	// 使用当前时间
	ext2.LocalTime = Time.Now().TotalMs();
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

	cfg->Server	= msg.Server;
	cfg->ServerPort = msg.Port;
	cfg->VisitToken	= msg.VisitToken;

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

    auto socket = dynamic_cast<ISocket*>(Control->Port);
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
	reg.Pass	= Cfg->Pass;

	auto now	= Sys.Ms();
	reg.Salt	= Buffer(&now, 8);

	reg.Show(true);

	TokenMessage msg(7);
	reg.WriteMessage(msg);
	Send(msg);
}

void TokenClient::OnRegister(TokenMessage& msg ,Controller* ctrl)
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
	cfg->User	= reg.User;
	cfg->Pass	= reg.Pass;

	cfg->Show();
	cfg->Save();

	Status	= 0;

	auto ctrl2 = dynamic_cast<TokenController*>(ctrl);
	if (ctrl2) ctrl2->Key.SetLength(0);

	Sys.SetTask(_task, true, 0);
}

// 登录
void TokenClient::Login()
{
	TS("TokenClient::Login");

	LoginMessage login;

	auto cfg	= Cfg;
	login.User	= cfg->User;
	//login.Pass	= MD5::Hash(cfg->Pass);

	// 原始密码对盐值进行加密，得到登录密码
	auto now	= Sys.Ms();
	auto arr	= Buffer(&now, 8);
	login.Salt	= arr;
	RC4::Encrypt(arr, cfg->Pass);
	login.Pass	= arr.ToHex();

	TokenMessage msg(2);
	login.WriteMessage(msg);
	login.Show(true);

	Send(msg);
}

bool TokenClient::OnLogin(TokenMessage& msg, Controller* ctrl)
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
		auto tctrl = dynamic_cast<TokenController*>(ctrl);
		tctrl->Token = Token;
		logMsg.Show(true);
		debug_printf("令牌：0x%08X ", Token);
		if (logMsg.Key.Length())
		{
			auto ctrl2 = dynamic_cast<TokenController*>(ctrl);
			if (ctrl2) ctrl2->Key = logMsg.Key;

			debug_printf("通信密码：");
			logMsg.Key.Show();
		}

		debug_printf("\r\n");
	}

	return true;
}

bool TokenClient::OnLocalLogin(TokenMessage& msg, Controller* ctrl)
{
	if(msg.Reply) return false;

	TS("TokenClient::OnLocalLogin");
	auto ctrl2		= dynamic_cast<TokenController*>(ctrl);

	auto rs	= msg.CreateReply();

	LoginMessage login;
	// 这里需要随机密匙
	login.Key		= ctrl2->Key;
	// 随机令牌
	login.Token		= Sys.Ms();
	login.Reply		= true;
	login.WriteMessage(rs);

	Reply(rs, ctrl);


	//ctrl2->Key.Copy(0, login.User, 0, -1);
	 ctrl2->Token 	= login.Token;

	return true;
}

// Ping指令用于保持与对方的活动状态
void TokenClient::Ping()
{
	TS("TokenClient::Ping");

	if(LastActive > 0 && LastActive + 30000 < Sys.Ms())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Hello任务
		debug_printf("30秒无法联系，服务端可能已经掉线，重新开始握手\r\n");

		auto ctrl2 = dynamic_cast<TokenController*>(Control);
		if (ctrl2) ctrl2->Key.SetLength(0);

		Status = 0;

		return;
	}

	TokenPingMessage pinMsg;

	TokenMessage msg(3);
	pinMsg.WriteMessage(msg);

	Send(msg);
}

bool TokenClient::OnPing(TokenMessage& msg, Controller* ctrl)
{
	TS("TokenClient::OnPing");

	if (!msg.Reply) return false;

	TokenPingMessage pinMsg;
	pinMsg.ReadMessage(msg);
	UInt64 start = pinMsg.LocalTime;

	UInt64 now = Time.Now().TotalMs();
	int cost 	= (int)(now - start);
	if(cost < 0) cost = -cost;
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
void TokenClient::OnRead(const TokenMessage& msg, Controller* ctrl)
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
void TokenClient::OnWrite(const TokenMessage& msg, Controller* ctrl)
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

void TokenClient::OnInvoke(const TokenMessage& msg, Controller* ctrl)
{

}
