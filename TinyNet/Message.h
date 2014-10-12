#ifndef __Message_H__
#define __Message_H__

#include "Sys.h"
#include "List.h"
#include "Net\ITransport.h"
#include "Stream.h"
#include "Timer.h"

// 消息
// 头部按照内存布局，但是数据和校验部分不是
// 请求 0038-0403-0000-BC4C，从0x38发往0（广播），功能4，标识3（保留字段用于业务），序号0长度0，校验0x4CBC（小字节序）
// 响应 3856-048x-0000-xxxx
// 错误 3856-044x-0000
// 负载 0038-1000-0003-030303-A936，从0x38广播，功能4，长度3，负载03-03-03
class Message
{
public:
	// 标准头部，符合内存布局。注意凑够4字节，否则会有很头疼的对齐问题
	byte Dest;		// 目的地址
	byte Src;		// 源地址
	byte Code;		// 功能代码
	byte Flags:5;	// 标识位。也可以用来做二级命令
	byte Confirm:1;	// 是否需要确认响应
	byte Error:1;	// 是否错误
	byte Reply:1;	// 是否响应
	byte Sequence;	// 序列号
	byte Length;	// 数据长度
	ushort Checksum;// 16位检验和

	// 负载数据及校验部分，并非内存布局。
	ushort Crc16;	// 整个消息的Crc16校验，计算前Checksum清零
	byte Data[64];	// 数据部分

public:
	// 初始化消息，各字段为0
	Message(byte code = 0);
	// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
	bool Parse(MemoryStream& ms);
	// 验证消息校验和是否有效
	bool Verify();
	// 计算当前消息的Crc
	void ComputeCrc();
	// 设置数据
	void SetData(byte* buf, uint len);

	// 写入指定数据流
	void Write(MemoryStream& ms);
};

// 消息头大小
#define MESSAGE_SIZE offsetof(Message, Checksum) + 2

class Controller
{
private:
	FixedArray<ITransport, 4>	_ports;	// 数据传输口
	//int				_portCount;	// 传输口个数
	uint			_Sequence;	// 控制器的消息序号
	Timer*			_Timer;		// 用于错误重发机制的定时器

	static uint OnReceive(ITransport* transport, byte* buf, uint len, void* param);

	class QueueNode
	{
	public:
		ulong		Expired;// 过期时间
		Message*	Msg;	// 消息
		ITransport*	Port;
		byte*		Buf;
		uint		Length;
		bool		Success;

		QueueNode() { }

		QueueNode(const QueueNode& queue)
		{
			Expired	= queue.Expired;
			Msg		= queue.Msg;
			Port	= queue.Port;
			Buf		= queue.Buf;
			Length	= queue.Length;
			Success	= queue.Success;
		}
	};
	LinkedList<QueueNode*> _Queue;

	bool SendInternal(Message& msg, ITransport* port);
	static void SendTask(void* sender, void* param);
	void SendTask();

public:
	byte Address;	// 本地地址
	
	void Init();
	Controller(ITransport* port);
	Controller(ITransport* ports[], int count);
	~Controller();

	void AddTransport(ITransport* port);

	// 发送消息，传输口参数为空时向所有传输口发送消息
	uint Send(byte dest, byte code, byte* buf = NULL, uint len = 0, ITransport* port = NULL);
	// 发送消息，传输口参数为空时向所有传输口发送消息
	bool Send(Message& msg, ITransport* port = NULL, uint msTimeout = 0);
	bool SendSync(Message& msg, uint msTimeout = 10);
	// 回复对方的请求消息
	bool Reply(Message& msg, ITransport* port = NULL);
	bool Error(Message& msg, ITransport* port = NULL);

protected:
	bool Process(MemoryStream& ms, ITransport* port);

// 处理器部分
public:
	// 处理消息，返回是否成功
	typedef bool (*CommandHandler)(Message& msg, void* param);

private:
    struct CommandHandlerLookup
    {
        uint			Code;	// 代码
        CommandHandler	Handler;// 处理函数
		void*			Param;	// 参数
    };
	CommandHandlerLookup* _Handlers[16];
	byte _HandlerCount;

public:
	// 注册消息处理器。考虑到业务情况，不需要取消注册
	void Register(byte code, CommandHandler handler, void* param = NULL);

// 常用系统级消息
public:
	// 询问及设置系统时间
	static bool SysTime(Message& msg, void* param);
	// 询问系统标识号
	static bool SysID(Message& msg, void* param);
	// 广播发现系统
	static bool Discover(Message& msg, void* param);

// 测试部分
public:
	static void Test(ITransport* port);
};

#endif
