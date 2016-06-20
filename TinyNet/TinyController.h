#ifndef __TinyController_H__
#define __TinyController_H__

#include "Message\Controller.h"

#include "TinyMessage.h"


// 环形队列。记录收到消息的序列号，防止短时间内重复处理消息
class RingQueue
{
public:
	int		Index;
	ushort	Arr[32];
	UInt64	Expired;	// 过期时间，微秒

	RingQueue();
	void	Push(ushort item);
	int		Find(ushort item) const;

	bool	Check(ushort item);
};

// 统计信息
class TinyStat
{
public:
	uint	Msg;	// 总消息数
	uint	Send;	// 总次数。每条消息可能发送多次
	uint	Success;// 总成功。有多少消息收到确认，每条消息仅计算一次确认
	uint	Bytes;	// 总字节数。成功发送消息的字节数
	uint	Cost;	// 总开销ms。成功发送消息到收到确认所花费的时间
	uint	Receive;// 收到消息数
	uint	Reply;	// 发出的响应
	uint	Broadcast;	// 广播

	TinyStat();
	
	// 重载等号运算符
    TinyStat& operator=(const TinyStat& ts);
	
	void Clear();
};

class MessageNode;

// 消息控制器。负责发送消息、接收消息、分发消息
class TinyController : public Controller
{
private:
	MessageNode*	_Queue;	// 消息队列。允许多少个消息同时等待响应

	RingQueue	_Ring;		// 环形队列
	uint		_taskID;	// 发送队列任务

	void AckRequest(const TinyMessage& msg);	// 处理收到的Ack包
	bool AckResponse(const TinyMessage& msg);	// 向对方发出Ack包

protected:
	virtual bool Dispatch(Stream& ms, Message* pmsg, void* param);
	// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
	virtual bool Valid(const Message& msg);

public:
	byte	Address;	// 本地地址
	byte	Mode;		// 接收模式。0只收自己，1接收自己和广播，2接收所有。默认0
	ushort	Interval;	// 队列发送间隔，默认10ms
	short	Timeout;	// 队列发送超时，默认50ms。如果不需要超时重发，那么直接设置为-1
	byte	QueueLength;// 队列长度，默认8

	byte	NoLogCodes[8];	// 没有日志的指令

	TinyController();
	virtual ~TinyController();

	void ApplyConfig();
	virtual void Open();

	// 发送消息
	virtual bool Send(Message& msg);
	// 回复对方的请求消息
	virtual bool Reply(Message& msg);
	// 广播消息，不等待响应和确认
	bool Broadcast(TinyMessage& msg);

	// 放入发送队列，超时之前，如果对方没有响应，会重复发送
	bool Post(const TinyMessage& msg, int msTimeout = -1);

	// 循环处理待发送的消息队列
	void Loop();

	// 获取密钥的回调
	//void (*GetKey)(byte id, Buffer& key, void* param);
	Delegate2<byte, Buffer&>	GetKey;

public:
	// 统计。平均值=(LastCost + TotalCost)/(LastSend + TotalSend)。每一组完成以后，TotalXXX整体复制给LastXXX
	TinyStat	Total;	// 总统计
	TinyStat	Last;	// 最后一次统计

private:
	// 显示统计信息
	void ShowStat() const;

	void ShowMessage(const TinyMessage& msg, bool send, const ITransport* port);
};

// 消息队列。需要等待响应的消息，进入消息队列处理。
class MessageNode
{
public:
	byte	Using;		// 是否在使用
	byte	Seq;		// 序列号
	byte	Length;
	byte	Times;		// 发送次数
	byte	Data[64];
	byte	Mac[6];		// 物理地址
	UInt64	StartTime;	// 开始时间ms
	UInt64	EndTime;	// 过期时间ms
	UInt64	Next;		// 下一次重发时间ms
	//UInt64	LastSend;	// 最后一次发送时间ms

	void Set(const TinyMessage& msg, int msTimeout);
};

#endif
