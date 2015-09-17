#ifndef __Controller_H__
#define __Controller_H__

#include "Sys.h"
#include "List.h"
#include "Net\ITransport.h"

#include "Message.h"

// 处理消息，返回是否成功
typedef bool (*MessageHandler)(Message& msg, void* param);

// 消息控制器。负责发送消息、接收消息、分发消息
class Controller
{
private:
	static uint Dispatch(ITransport* port, ByteArray& bs, void* param, void* param2);

protected:

	virtual bool Dispatch(Stream& ms, Message* pmsg);
	// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
	virtual bool Valid(const Message& msg);
	// 接收处理
	virtual bool OnReceive(Message& msg);

public:
	ITransport*	Port;		// 数据传输口数组
	ushort		MaxSize;	// 最大消息大小
	byte		MinSize;	// 最小消息大小
	bool 		Opened;

	Controller();
	virtual ~Controller();

	virtual void Open();
	virtual void Close();

	// 发送消息，传输口参数为空时向所有传输口发送消息
	virtual bool Send(Message& msg);
	// 回复对方的请求消息
	virtual bool Reply(Message& msg);

	// 收到消息时触发
	MessageHandler	Received;
	void*			Param;
};

#endif
