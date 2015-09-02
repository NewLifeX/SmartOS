#ifndef __TinyMessage_H__
#define __TinyMessage_H__

#include "Sys.h"
#include "List.h"
#include "Net\ITransport.h"
#include "Stream.h"

#include "Message\Controller.h"

// 消息
// 头部按照内存布局，但是数据和校验部分不是
// 请求 0038-0403-0000-BC4C，从0x38发往0（广播），功能4，标识3（保留字段用于业务），序号0长度0，校验0x4CBC（小字节序）
// 响应 3856-048x-0000-xxxx
// 错误 3856-044x-0000
// 负载 0038-1000-0003-030303-A936，从0x38广播，功能4，长度3，负载03-03-03
class TinyMessage : public Message
{
private:

public:
	// 标准头部，符合内存布局。注意凑够4字节，否则会有很头疼的对齐问题
	byte Dest;		// 目的地址
	byte Src;		// 源地址
	byte _Code;		// 功能代码
	byte Flags:3;	// 标识位。也可以用来做二级命令
	byte UseTTL:1;	// 使用TTL。具体TTL值位于数据包最后
	byte NoAck:1;	// 是否不需要确认包
	byte Ack:1;		// 确认包
	byte _Error:1;	// 是否错误
	byte _Reply:1;	// 是否响应
	byte Sequence;	// 序列号
	byte _Length;	// 数据长度
	byte _Data[32];	// 数据部分
	ushort Checksum;// 16位检验和

	// 可选的附加数据紧跟在头数据后面，可能直接读取内存区域
	byte TTL;		// 路由生命周期。为方便路由，不参与Crc校验
#if DEBUG
	byte Retry;		// 调试诊断模式下该字段表示第几次重发
#endif

	// 负载数据及校验部分，并非内存布局。
	ushort Crc;		// 整个消息的Crc16校验，计算前Checksum清零

	static const uint HeaderSize = 1 + 1 + 1 + 1 + 1 + 1;	// 消息头部大小
	static const uint MinSize = HeaderSize + 0 + 2;	// 最小消息大小

public:
	// 初始化消息，各字段为0
	TinyMessage(byte code = 0);

	// 消息所占据的指令数据大小。包括头部、负载数据和附加数据
	virtual uint Size() const;

	// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
	virtual bool Read(Stream& ms);
	// 写入指定数据流
	virtual void Write(Stream& ms);

	// 验证消息校验码是否有效
	virtual bool Valid() const;
	// 计算当前消息的Crc
	virtual void ComputeCrc();

	// 显示消息内容
	virtual void Show() const;
};

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

// 统计信息
class TinyStat
{
public:
	uint	Msg;	// 总消息数
	uint	Send;	// 总次数。每条消息可能发送多次
	uint	Ack;	// 总成功。有多少消息收到确认，每条消息仅计算一次确认
	uint	Bytes;	// 总字节数。成功发送消息的字节数
	uint	Cost;	// 总开销。成功发送消息到收到确认所花费的时间
	//uint	Retry;	// 总重试次数
	int		Receive;// 收到消息数

	TinyStat()
	{
		memset(this, 0, sizeof(this[0]));
	}
};

// 消息队列。需要等待响应的消息，进入消息队列处理。
class MessageNode
{
public:
	byte	Using;		// 是否在使用
	byte	Sequence;	// 序列号
	byte	Data[32];
	uint	Length;
	uint	Period;		// 延迟间隔。每次逐步递增
	ulong	StartTime;	// 开始时间
	ulong	Next;		// 下一次重发时间
	ulong	Expired;	// 过期时间，微秒
	uint	Times;		// 发送次数
	ulong	LastSend;	// 最后一次发送时间

	void SetMessage(TinyMessage& msg);
};

// 消息控制器。负责发送消息、接收消息、分发消息
class TinyController : public Controller
{
private:
	MessageNode	_Queue[16];	// 消息队列。最多允许16个消息同时等待响应

	uint		_Sequence;	// 控制器的消息序号
	RingQueue	_Ring;		// 环形队列
	uint		_taskID;	// 发送队列任务

	void AckRequest(const TinyMessage& msg);	// 处理收到的Ack包
	void AckResponse(const TinyMessage& msg);	// 向对方发出Ack包

protected:
	virtual bool Dispatch(Stream& ms, Message* pmsg);
	// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
	virtual bool Valid(const Message& msg);

public:
	byte	Address;	// 本地地址
	bool	NoAck;		// 是否不使用Ack，若为true，即使收到消息要求Ack也不发送Ack。默认false
	uint	Interval;	// 消息队列发送间隔，8000微秒
	int		Timeout;	// 消息队列发送消息的默认超时时间，50000微秒。如果不需要超时重发，那么直接设置为-1

	TinyController();
	virtual ~TinyController();

	virtual void Open();

	// 发送消息，传输口参数为空时向所有传输口发送消息
	virtual bool Send(Message& msg);

	// 发送消息，传输口参数为空时向所有传输口发送消息
	uint Post(byte dest, byte code, byte* buf = NULL, uint len = 0);
	// 把消息放入发送队列，usTimeout微秒超时时间内，如果对方没有响应，会重复发送
	bool Post(TinyMessage& msg, int usTimeout = -1);
	// 回复对方的请求消息
	virtual bool Reply(Message& msg);
	// 广播消息，不等待响应和确认
	bool Broadcast(TinyMessage& msg);

	// 循环处理待发送的消息队列
	void Loop();

public:
	// 统计。平均值=(LastCost + TotalCost)/(LastSend + TotalSend)。每一组完成以后，TotalXXX整体复制给LastXXX
	TinyStat	Total;	// 总统计
	TinyStat	Last;	// 最后一次统计

	// 显示统计信息
	void ShowStat();
};

#endif

/*
	微网消息协议

微网协议是一个针对微型广播网进行通讯而开发的协议，网络节点最大255个，消息最大32字节。
每一个网络节点随时都可以发送消息，而可能会出现冲突，因此要求各节点快速发送小数据包，并且要有错误重发机制。
每个消息都有序列号，以免收到重复消息，特别是在启用错误重发机制时。

消息类型：
1，普通请求。Reply=0 NoAck=0，对方收到后处理业务逻辑，然后普通响应，不回复也行。用于不重要的数据包，比如广播。
2，普通响应。Reply=1 NoAck=0，收到普通请求，处理业务逻辑后响应。也不管对方有没有收到。
3，增强请求。Reply=0 NoAck=1，对方收到后马上发出Ack，告诉发送方已确认收到，否则发送方认为出错进行重发。
4，增强响应。Reply=1 NoAck=1，业务处理后做出响应，要求对方发送Ack确认收到该数据包，否则出错重发。

显然，增强请求将会收到两个响应，一个为Ack，另一个带业务数据负载，当然后一个也可以取消。
控制器不会把Ack的响应传递给业务层。

*/
