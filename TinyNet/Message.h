#ifndef __Message_H__
#define __Message_H__

#include "Sys.h"
#include "List.h"
#include "Net\ITransport.h"
#include "Stream.h"
#include "Timer.h"

// 消息基类
class Message
{
public:
	byte	Code;	// 消息代码
	byte	Length;	// 数据长度
	byte*	Data;	// 数据。指向子类内部声明的缓冲区

	// 初始化消息，各字段为0
	Message(byte code = 0);
	Message(Message& msg);

	// 消息所占据的指令数据大小。包括头部、负载数据、校验和附加数据
	virtual uint Size() const = 0;

	// 分析数据，转为消息。
	virtual bool Read(MemoryStream& ms) = 0;
	// 写入指定数据流
	virtual void Write(MemoryStream& ms) = 0;

	// 验证消息校验码是否有效
	virtual bool Valid() const = 0;
	// 计算当前消息的Crc
	virtual void ComputeCrc() = 0;
	// 设置数据
	void SetData(byte* buf, uint len);
};

#endif
