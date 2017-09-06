#ifndef __LinkClient_H__
#define __LinkClient_H__

#include "Kernel\Sys.h"

#include "Net\ITransport.h"
#include "Net\Socket.h"

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

	ushort	PingTime;	// 心跳时间。秒
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
	bool Reply(const String& action, int seq, int code, const String& result);

	void Read(int start, int size);
	void Write(int start, const Buffer& bs);
	void Write(int start, byte dat);

	// 异步上传并等待响应，返回实际上传字节数
	int WriteAsync(int start, const Buffer& bs, int msTimeout);

	// 必须满足 start > 0 才可以。
	void ReportAsync(int start, uint length = 1);

	// 收到功能消息时触发
	//MessageHandler	Received;
	void*			Param;

	// 登录
	void Login();

	// 心跳，用于保持与对方的活动状态
	void Ping();

	// 重置并上报
	void Reset(const String& reason);
	void Reboot(const String& reason);

	// 快速建立客户端，注册默认Api
	static LinkClient* Create(cstring server, const Buffer& store);

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
	int		ReportStart;	// 下次上报偏移，-1不动
	byte	ReportLength;	// 下次上报数据长度

	void LoopTask();
	void CheckNet();
	bool CheckReport();

	static uint Dispatch(ITransport* port, Buffer& bs, void* param, void* param2);
};

#endif
