#ifndef __TinyLink_H__
#define __TinyLink_H__

#include "Kernel\Sys.h"

#include "Net\ITransport.h"

#include "Message\DataStore.h"
#include "Message\Json.h"

#include "LinkMessage.h"

// 微联客户端。串口、无线通信
class TinyLink
{
public:
	bool	Opened;
	bool	Logined;

	ushort	PingTime;	// 心跳时间。秒

	ITransport*	Port;	// 传输口
	DataStore	Store;	// 数据存储区

	TinyLink();

	void Open();	// 打开连接，登录并定时心跳
	void Close();	// 关闭连接
	void Listen();	// 打开并监听连接，作为服务端，没有登录和心跳

	// 发送消息
	bool Send(LinkMessage& msg, const String& data);
	bool Invoke(const String& action, const Json& args);
	bool Reply(const String& action, int seq, int code, const String& result);

	// 登录
	void Login();
	void OnLogin(LinkMessage& msg);

	// 心跳，用于保持与对方的活动状态
	void Ping();
	void OnPing(LinkMessage& msg);

	// 快速建立客户端，注册默认Api
	static TinyLink* Create(ITransport& port, const Buffer& store);

private:
	uint	_task;

	void LoopTask();

	void OnReceive(LinkMessage& msg);
	static uint Dispatch(ITransport* port, Buffer& bs, void* param, void* param2);
};

#endif
