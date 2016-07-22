#include "TTime.h"

#include "Net\Socket.h"

#include "Message\BinaryPair.h"

#include "TokenSession.h"

#include "TokenMessage.h"
#include "HelloMessage.h"
#include "LoginMessage.h"
#include "RegisterMessage.h"
#include "TokenDataMessage.h"
#include "ErrorMessage.h"

#include "Security\RC4.h"
#include "Security\Crc.h"

TokenSession::TokenSession(TokenClient& client, TokenController& ctrl) :
	Client(client),
	Control(ctrl)
{
	client.Sessions.Add(this);
	Status = 0;
}

TokenSession::~TokenSession()
{
	Client.Sessions.Remove(this);
}

bool TokenSession::Send(TokenMessage& msg)
{
	// 未登录之前，只能 握手、登录、注册
	if (Token == 0)
	{
		if (msg.Code != 0x01 && msg.Code != 0x02 && msg.Code != 0x07) return false;
	}
	assert(&Control, "还没有Control呢");

	msg.State = &Remote;

	return Control.Send(msg);
}

void TokenSession::OnReceive(TokenMessage& msg)
{
	TS("TokenSession::OnReceive");

	LastActive = Sys.Ms();
	if (Token == 0 && msg.Code > 1)
	{
		auto rs = msg.CreateReply();

		rs.Code = 0x01;
		rs.Error = true;

		HelloMessage ext;
		ext.ErrCode = 0xFF;

		ext.WriteMessage(rs);

		Control.Reply(rs);
		return;
	}

	switch (msg.Code)
	{
	case 0x01:
		OnHello(msg);
		break;
	case 0x02:
		OnLogin(msg);
		break;
	case 0x03:
		OnPing(msg);
		break;
	case 0x05:
	case 0x06:
	case 0x08:
		// 读写和调用，交给客户端处理
		Client.OnReceive(msg, Control);
		break;
	}
}

bool TokenSession::OnHello(TokenMessage& msg)
{
	if (msg.Reply) return false;

	// 解析数据
	HelloMessage ext;
	ext.Reply = msg.Reply;

	ext.ReadMessage(msg);
	ext.Show(true);

	TS("TokenSession::OnHello");

	auto rs = msg.CreateReply();

	HelloMessage ext2;
	ext2.Reply = true;

	// 同一个控制器共用密码，如果还没有则新建一个
	auto& key = Control.Key;
	if (key.Length() == 0)
	{
		auto now = Sys.Ms();
		auto crc = Crc::Hash(Buffer(&now, 8));
		key = Buffer(&crc, 4);
		key.Show(true);
		//通知其它内网，密码被修改了
	}
	ext2.Key = key;

	auto sock = Control.Socket;
	ext2.EndPoint.Address = sock->Host->IP;
	ext2.EndPoint.Port = sock->Local.Port;

	ext2.Cipher = "RC4";
	ext2.Name = Client.Cfg->User();
	// 未注册时采用系统名称
	if (!ext2.Name)
	{
		ext2.Name = Sys.Name;
		ext2.Key = Buffer(Sys.ID, 16);
	}

	// 使用当前时间
	ext2.LocalTime = DateTime::Now().TotalMs();
	ext2.WriteMessage(rs);

	//Client.Reply(rs, Control);
	Control.Reply(rs);
	Status = 1;

	return true;
}

bool TokenSession::OnLogin(TokenMessage& msg)
{
	if (msg.Reply) return false;

	TS("TokenSession::OnLogin");

	auto rs = msg.CreateReply();

	Token = Sys.Ms();

	LoginMessage login;
	login.Key = Control.Key;
	login.Token = Token;
	login.Reply = true;
	login.WriteMessage(rs);

	Control.Reply(rs);

	Status = 2;
	Control.Token = Token;

	return true;
}

bool TokenSession::OnPing(TokenMessage& msg)
{
	TS("TokenSession::OnPing");

	if (msg.Reply) return false;

	auto rs = msg.CreateReply();

	Control.Reply(rs);

	Status = 3;
	return true;
}
