#ifndef __TokenClient_H__
#define __TokenClient_H__

#include "Sys.h"
#include "TokenNet\TokenConfig.h"
#include "TokenMessage.h"
#include "HelloMessage.h"

#include "TokenNet\TokenController.h"

#include "Message\DataStore.h"
#include "Message\BinaryPair.h"

class TokenSession;

// 微网客户端
class TokenClient
{
public:
	uint	Token;	// 令牌
	bool	Opened;
	int		Status;	// 状态。0准备、1握手完成、2登录后

	UInt64	LoginTime;	// 登录时间ms
	UInt64	LastActive;	// 最后活跃时间ms
	int		Delay;		// 心跳延迟。一条心跳指令从发出到收到所花费的时间
	int		MaxNotActive;	// 最大不活跃时间ms，超过该时间时重启系统。默认0

	List	Controls;	// 控制器集合
	List	Sessions;	// 会话集合
	TokenConfig*	Cfg;
	DataStore	Store;	// 数据存储区
	Dictionary	Routes;	// 路由集合

	TokenClient();

	void Open();		// 打开客户端
	void Close();

	// 发送消息
	bool Send(TokenMessage& msg, TokenController* ctrl = nullptr);
	bool Reply(TokenMessage& msg, TokenController* ctrl = nullptr);
	void OnReceive(TokenMessage& msg, TokenController& ctrl);

	// 收到功能消息时触发
	MessageHandler	Received;
	void*			Param;

// 常用系统级消息
	// 握手广播
	void SayHello(bool broadcast);

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

	// 远程调用委托。传入参数名值对以及结果缓冲区引用，业务失败时返回false并把错误信息放在结果缓冲区
	typedef bool (*InvokeHandler)(void* param, const BinaryPair& args, Stream& result);
	// 注册远程调用处理器
	void Register(cstring action, InvokeHandler handler, void* param = nullptr);
	// 模版支持成员函数
	template<typename T>
	void Register(cstring action, bool(T::*func)(const BinaryPair&, Stream&), T* target)
	{
		Register(action, *(InvokeHandler*)&func, target);
	}

private:
	bool OnHello(TokenMessage& msg, TokenController* ctrl);
	bool OnLocalHello(TokenMessage& msg, TokenController* ctrl);

	// 跳转
	bool OnRedirect(HelloMessage& msg);

	void OnRegister(TokenMessage& msg, TokenController* ctrl);

	bool OnLogin(TokenMessage& msg, TokenController* ctrl);
	bool OnLocalLogin(TokenMessage& msg,TokenController* ctrl);

	bool OnPing(TokenMessage& msg, TokenController* ctrl);
	bool ChangeIPEndPoint(const String& domain, ushort port);

	void OnRead(const TokenMessage& msg, TokenController* ctrl);
	void OnWrite(const TokenMessage& msg, TokenController* ctrl);

	void OnInvoke(const TokenMessage& msg, TokenController* ctrl);
	bool OnInvoke(const String& action, const BinaryPair& args, Stream& result);

private:
	uint	_task;
	uint	_taskBroadcast;	// 广播任务
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
