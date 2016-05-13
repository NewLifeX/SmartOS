#ifndef __TokenClient_H__
#define __TokenClient_H__

#include "Sys.h"
#include "TokenNet\TokenConfig.h"
#include "TokenMessage.h"
#include "HelloMessage.h"

#include "TokenNet\TokenController.h"

#include "Message\DataStore.h"

class TokenSession;

// 微网客户端
class TokenClient
{
private:
	uint	_task;

public:
	uint	Token;	// 令牌

	int		Status;	// 状态。0准备、1握手完成、2登录后

	UInt64	LoginTime;	// 登录时间ms
	UInt64	LastActive;	// 最后活跃时间ms
	int		Delay;		// 心跳延迟。一条心跳指令从发出到收到所花费的时间

	Controller* Control;
	TokenConfig*	Cfg;
	DataStore	Store;	// 数据存储区

	TokenClient();

	void Open();		// 打开客户端
	void Close();

	// 发送消息
	bool Send(TokenMessage& msg, Controller* ctrl = nullptr);
	bool Reply(TokenMessage& msg, Controller* ctrl = nullptr);
	bool OnReceive(TokenMessage& msg, Controller* ctrl);

	// 收到功能消息时触发
	MessageHandler	Received;
	void*			Param;

// 本地网络支持
	Controller* Local;			// 本地网络控制器
	TArray<TokenSession*> Sessions;	// 会话集合

// 常用系统级消息
	// 握手广播
	HelloMessage	Hello;
	void SayHello(bool broadcast = false, int port = 0);

	// 注册
	void Register();

	// 登录
	void Login();

	// Ping指令用于保持与对方的活动状态
	void Ping();

	void Read(int start, int size);
	void Write(int start, const Buffer& bs);

	// 远程调用
	void Invoke(const String& action, const Buffer& bs);

private:
	bool OnHello(TokenMessage& msg, Controller* ctrl);
	// 跳转
	bool OnRedirect(HelloMessage& msg);

	void OnRegister(TokenMessage& msg, Controller* ctrl);

	bool OnLogin(TokenMessage& msg, Controller* ctrl);
	bool OnLocalLogin(TokenMessage& msg,Controller* ctrl);

	bool OnPing(TokenMessage& msg, Controller* ctrl);
	bool ChangeIPEndPoint(const String& domain, ushort port);

	void OnRead(const TokenMessage& msg, Controller* ctrl);
	void OnWrite(const TokenMessage& msg, Controller* ctrl);

	void OnInvoke(const TokenMessage& msg, Controller* ctrl);
};

// 令牌会话
class TokenSession
{
public:
	TokenClient*	Client;

	uint	Token;		// 令牌
	ByteArray	ID;		// 设备标识
	ByteArray	Key;	// 密钥
	String		Name;	// 名称
	ushort	Version;	// 版本
	IPEndPoint	Addr;	// 地址

	int		Status;		// 状态。0准备、1握手完成、2登录后
	UInt64	LoginTime;	// 登录时间ms
	UInt64	LastActive;	// 最后活跃时间ms
};

#endif
