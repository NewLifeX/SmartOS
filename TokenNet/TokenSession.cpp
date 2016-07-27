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


#if DEBUG
uint TokenSession::StatShowTaskID = 0;
uint TokenSession::HisSsNum = 0;

uint SessionStat::BraHello = 0;
uint SessionStat::DecError = 0;

SessionStat::SessionStat() { Clear(); }
SessionStat::~SessionStat(){ }

void SessionStat::Clear()
{
	OnHello = 0;
	OnLogin = 0;
	OnPing	= 0;
}

String& SessionStat::ToStr(String& str) const
{
	str = str + "OnHello: " + OnHello + " OnLogin: " + OnLogin + " OnPing: " + OnPing;
	return str;
}
#endif


TokenSession::TokenSession(TokenClient& client, TokenController& ctrl) :
	Client(client),
	Control(ctrl)
{
#if DEBUG
	HisSsNum++;			// 历史Session个数
#endif
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
	// if (Token == 0 && msg.Code > 1 && Key.Length() == 0)
	if ((Token == 0 && msg.Code > 2) || msg.ErrorCode == DecryptError)	// 解密失败 直接让他重新来过
	{
		auto rs = msg.CreateReply();

		rs.Code = 0x01;
		rs.Error = true;

		HelloMessage ext;
		ext.ErrCode = 0x7F;

		ext.WriteMessage(rs);

		Control.Reply(rs);

#if DEBUG
		SessionStat::DecError++;			// 历史Session个数
#endif
		return;
	}

	switch (msg.Code)
	{
	case 0x01:
#if DEBUG
		if (msg.OneWay)
			SessionStat::BraHello++;
		else 
			Stat.OnHello++;
#endif
		OnHello(msg);
		break;
	case 0x02:
#if DEBUG
		Stat.OnLogin++;
#endif
		OnLogin(msg);
		break;
	case 0x03:
#if DEBUG
		Stat.OnPing++;
#endif
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
	// 未注册时采用系统名称    内网不考虑此问题
	// if (!ext2.Name)
	// {
	// 	ext2.Name = Sys.Name;
	// 	ext2.Key = Buffer(Sys.ID, 16);
	// }

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
#if DEBUG
String& TokenSession::ToStr(String& str) const
{
	int timeSec = (Sys.Ms() - LastActive) / 1000UL;
	str = str + Remote + " "+ Name +" LastAct "+timeSec +"s ago \t";

	Stat.ToStr(str);
	return str;
}
#endif
