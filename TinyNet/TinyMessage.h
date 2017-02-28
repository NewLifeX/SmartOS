#ifndef __TinyMessage_H__
#define __TinyMessage_H__

#include "Kernel\Sys.h"
#include "Net\ITransport.h"

#include "Message\Message.h"

// 消息
// 头部按照内存布局，但是数据和校验部分不是
class TinyMessage : public Message
{
public:
	// 标准头部，符合内存布局。注意凑够4字节，否则会有很头疼的对齐问题
	byte Dest;		// 目的地址
	byte Src;		// 源地址
	byte _Code;		// 功能代码
	byte Retry:4;	// 重发次数。
	//byte TTL:1;		// 路由TTL。最多3次转发
	byte NoAck:1;	// 是否不需要确认包
	byte Ack:1;		// 确认包
	byte _Error:1;	// 是否错误
	byte _Reply:1;	// 是否响应
	byte Seq;		// 序列号
	byte _Length;	// 数据长度
	byte _Data[64];	// 数据部分
	ushort Checksum;// 16位检验和

	// 负载数据及校验部分，并非内存布局。
	ushort Crc;		// 整个消息的Crc16校验，计算前Checksum清零

	static const int HeaderSize = 1 + 1 + 1 + 1 + 1 + 1;	// 消息头部大小
	static const int MinSize = HeaderSize + 0 + 2;	// 最小消息大小

public:
	// 初始化消息，各字段为0
	TinyMessage(byte code = 0);

	// 消息所占据的指令数据大小。包括头部、负载数据和附加数据
	virtual int Size() const;
	// 数据缓冲区大小
	virtual int MaxDataSize() const;

	// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
	virtual bool Read(Stream& ms);
	// 写入指定数据流
	virtual void Write(Stream& ms) const;

	// 验证消息校验码是否有效
	virtual bool Valid() const;

	// 创建当前消息对应的响应消息。设置源地址目的地址、功能代码、序列号、标识位
	TinyMessage CreateReply() const;

	// 显示消息内容
	virtual void Show() const;
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
