#ifndef __LinkClient_H__
#define __LinkClient_H__

#include "Kernel\Sys.h"

#include "Message\DataStore.h"
#include "Message\Json.h"

#include "LinkMessage.h"
#include "LinkConfig.h"

// 物联客户端
class LinkClient
{
public:
	int		Status;	// 状态。0准备、1握手完成、2登录后
	bool	Opened;

	byte	PingTime;	// 心跳时间。秒
	UInt64	LoginTime;	// 登录时间ms
	UInt64	LastSend;	// 最后发送时间ms
	UInt64	LastActive;	// 最后活跃时间ms
	int		Delay;		// 心跳延迟。一条心跳指令从发出到收到所花费的时间
	int		MaxNotActive;	// 最大不活跃时间ms，超过该时间时重启系统。默认0

	LinkConfig*	Cfg;

	Socket*		Master;		// 主链接。服务器长连接
	DataStore	Store;	// 数据存储区
	Dictionary<cstring, IDelegate*>	Routes;	// 路由集合

	LinkClient();

	void Open();
	void Close();

	// 发送消息
	bool Invoke(const String& action, const Json& args);
	bool Reply(const String& action, int seq, int code, const Json& result);

	// 收到功能消息时触发
	//MessageHandler	Received;
	void*			Param;

	// 登录
	void Login();

	// 心跳，用于保持与对方的活动状态
	void Ping();

	static LinkClient* Current;

private:
	void OnReceive(LinkMessage& msg);
	bool Send(const LinkMessage& msg);

	void OnLogin(LinkMessage& msg);
	void OnPing(LinkMessage& msg);

	void OnRead(LinkMessage& msg);
	void OnWrite(LinkMessage& msg);

private:
	uint	_task;

	void LoopTask();
	void CheckNet();

	static uint Dispatch(ITransport* port, Buffer& bs, void* param, void* param2);
};

#endif
