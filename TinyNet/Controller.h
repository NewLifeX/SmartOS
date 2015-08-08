#ifndef __Controller_H__
#define __Controller_H__

#include "Sys.h"
#include "List.h"
#include "Net\ITransport.h"
#include "Stream.h"
#include "Timer.h"

#include "Message.h"

// 处理消息，返回是否成功
typedef bool (*MessageHandler)(Message& msg, void* param);

// 消息控制器。负责发送消息、接收消息、分发消息
class Controller
{
private:
	static uint Dispatch(ITransport* port, byte* buf, uint len, void* param);

protected:

	virtual bool Dispatch(Stream& ms, Message* pmsg);
	// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
	virtual bool Valid(Message& msg);
	// 接收处理
	virtual bool OnReceive(Message& msg);

public:
	ITransport*	Port;		// 数据传输口数组
	byte		MinSize;	// 最小消息大小
	byte		MaxSize;	// 最大消息大小
	bool 		Opened;

	Controller();
	virtual ~Controller();

	virtual void Open();
	virtual void Close();

	// 发送消息，传输口参数为空时向所有传输口发送消息
	virtual bool Send(Message& msg);
	// 回复对方的请求消息
	virtual bool Reply(Message& msg);

// 发送核心
protected:

// 处理器部分
private:
    struct HandlerLookup
    {
        uint			Code;	// 代码
        MessageHandler	Handler;// 处理函数
		void*			Param;	// 参数
    };
	List<HandlerLookup*> _Handlers;

public:
	// 注册消息处理器。考虑到业务情况，不需要取消注册
	void Register(byte code, MessageHandler handler, void* param = NULL);

	// 收到消息时触发
	MessageHandler	Received;
	void*			Param;
};

#endif
