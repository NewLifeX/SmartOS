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
	byte Flags:3;	// 标识位。也可以用来做二级命令
	byte UseTTL:1;	// 使用TTL。具体TTL值位于数据包最后
	byte NoAck:1;	// 是否不需要确认包
	byte Ack:1;		// 确认包
	byte Error:1;	// 是否错误
	byte Reply:1;	// 是否响应
	byte Sequence;	// 序列号
	byte Length;	// 数据长度
	ushort Checksum;// 16位检验和

	// 负载数据及校验部分，并非内存布局。
	ushort Crc16;	// 整个消息的Crc16校验，计算前Checksum清零
	byte Data[32];	// 数据部分

	byte TTL;		// 路由生命周期。为方便路由，不参与Crc校验

public:
	// 初始化消息，各字段为0
	Message(byte code = 0);
	Message(Message& msg);

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

class MessageNode;
class RingQueue;

// 环形队列。记录收到消息的序列号，防止短时间内重复处理消息
class RingQueue
{
public:
	int	Index;
	ushort Arr[32];
	ulong Expired;	// 过期时间，微秒

	RingQueue();
	void Push(ushort item);
	int Find(ushort item);

	bool Check(ushort item);
};

// 处理消息，返回是否成功
typedef bool (*MessageHandler)(Message& msg, void* param);

// 消息控制器。负责发送消息、接收消息、分发消息
class Controller
{
private:
	FixedArray<ITransport, 4>	_ports;	// 数据传输口
	FixedArray<MessageNode, 16>	_Queue;	// 消息队列。最多允许16个消息同时等待响应

	uint		_Sequence;	// 控制器的消息序号
	RingQueue	_Ring;		// 环形队列
	uint		_taskID;	// 发送队列任务

	void Init();
	void PreSend(Message& msg);	// 发送准备
	static uint Dispatch(ITransport* transport, byte* buf, uint len, void* param);
	bool Dispatch(MemoryStream& ms, ITransport* port);

public:
	byte	Address;	// 本地地址
	uint	Interval;	// 消息队列发送间隔，10毫秒
	uint	Timeout;	// 消息队列发送消息的默认超时时间，50毫秒

	Controller(ITransport* port);
	Controller(ITransport* ports[], int count);
	~Controller();

	void AddTransport(ITransport* port);

	// 发送消息，传输口参数为空时向所有传输口发送消息
	uint Send(byte dest, byte code, byte* buf = NULL, uint len = 0, ITransport* port = NULL);
	// 把消息放入发送队列，timerout毫秒重试超时时间，传输口参数为空时向所有传输口发送消息
	//bool Post(Message& msg, int timeout = 0, ITransport* port = NULL);
	// 发送消息，timerout毫秒超时时间内，如果对方没有响应，会重复发送
	bool Send(Message& msg, int expire = -1, ITransport* port = NULL);
	// 回复对方的请求消息
	bool Reply(Message& msg, ITransport* port = NULL);
	bool Error(Message& msg, ITransport* port = NULL);

	// 循环处理待发送的消息队列
	void Loop();

// 处理器部分
private:
    struct HandlerLookup
    {
        uint			Code;	// 代码
        MessageHandler	Handler;// 处理函数
		void*			Param;	// 参数
    };
	HandlerLookup* _Handlers[16];
	byte _HandlerCount;

public:
	// 注册消息处理器。考虑到业务情况，不需要取消注册
	void Register(byte code, MessageHandler handler, void* param = NULL);
};

// 消息队列。需要等待响应的消息，进入消息队列处理。
class MessageNode
{
public:
	FixedArray<ITransport, 4> Ports;	// 未收到响应消息的传输口
	//Message*	Msg;	// 消息
	byte		Sequence;	// 序列号
	byte		Data[32];
	uint		Length;
	ulong		Next;		// 下一次重发时间
	ulong		Expired;	// 过期时间，微秒
	//Message*	Reply;	// 匹配的响应消息

	//MessageNode();
	//~MessageNode();

	void SetMessage(Message& msg);
};

#endif

/*
	微网消息协议

微网协议是一个针对微型广播网进行通讯而开发的协议，网络节点最大255个，消息最大32字节。
每一个网络节点随时都可以发送消息，而可能会出现冲突，因此要求各节点快速发送小数据包，并且要有错误重发机制。
每个消息都有序列号，以免收到重复消息，特别是在启用错误重发机制时。

消息类型：
1，普通请求。Reply=0 Confirm=0，对方收到后处理业务逻辑，然后普通响应，不回复也行。用于不重要的数据包，比如广播。
2，普通响应。Reply=1 Confirm=0，收到普通请求，处理业务逻辑后响应。也不管对方有没有收到。
3，增强请求。Reply=0 Confirm=1，对方收到后马上做出负载为0的增强响应，告诉发送方已确认收到，否则发送方认为出错进行重发。
4，增强响应。Reply=1 Confirm=1，业务处理后做出响应，要求对方确认收到该数据包，否则出错重发。

显然，增强请求将会收到两个增强响应，一个为0负载，另一个带业务数据负载，当然后一个也可以取消。
控制器不会把0负载的增强响应传递给业务层。

*/
