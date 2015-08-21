#ifndef __TinyClient_H__
#define __TinyClient_H__

#include "Sys.h"
#include "TinyMessage.h"

#include "Message\DataStore.h"

// 微网客户端
class TinyClient
{
private:
	int			_TaskID;

public:
	TinyController* Control;

	byte		Server;		// 服务端地址
	ushort		Type;		// 设备类型。两个字节可做二级分类
	ByteArray	Password;	// 通讯密码

	ulong		LastActive;	// 最后活跃时间

	TinyClient(TinyController* control);

	void Open();
	void Close();

	// 发送消息
	bool Send(TinyMessage& msg);
	bool Reply(TinyMessage& msg);
	bool OnReceive(TinyMessage& msg);

	// 收到功能消息时触发
	MessageHandler	Received;
	void*			Param;

// 数据区
public:
	DataStore	Store;		// 数据存储区

	void Report(TinyMessage& msg);

private:
	void OnWrite(TinyMessage& msg);
	void OnRead(TinyMessage& msg);

// 常用系统级消息
public:
	// 组网
	ushort		TranID;		// 组网会话
	void Join();
	bool OnJoin(TinyMessage& msg);
	bool OnDisjoin(TinyMessage& msg);

	// 心跳
	void Ping();
	bool OnPing(TinyMessage& msg);
};

#endif
