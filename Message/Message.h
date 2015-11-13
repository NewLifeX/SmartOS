#ifndef __Message_H__
#define __Message_H__

#include "Sys.h"
#include "Stream.h"

// 消息基类
class Message
{
public:
	byte	Code;		// 消息代码
	byte	Reply:1;	// 是否响应指令
	byte	Error:1;	// 是否错误
	byte	Length;		// 数据长度
	byte*	Data;		// 数据。指向子类内部声明的缓冲区

	void*	State;		// 其它状态数据

	// 初始化消息，各字段为0
	Message(byte code = 0);

	// 消息所占据的指令数据大小。包括头部、负载数据、校验和附加数据
	virtual uint Size() const = 0;
	// 数据缓冲区大小
	virtual uint MaxDataSize() const = 0;

	// 从数据流中读取消息
	virtual bool Read(Stream& ms) = 0;
	// 把消息写入数据流中
	virtual void Write(Stream& ms) const = 0;

	// 验证消息校验码是否有效
	virtual bool Valid() const = 0;
	// 计算当前消息的Crc
	virtual void ComputeCrc() = 0;
	// 克隆对象
	virtual bool Clone(const Message& msg);

	// 设置数据
	void SetData(const void* buf, uint len, uint offset = 0);
	void SetData(const ByteArray& bs, uint offset = 0);
	void SetError(byte errorCode, const char* msg = NULL);

	// 负载数据转数据流
	Stream ToStream();
	Stream ToStream() const;
	// 负载数据转字节数组
	//ByteArray ToArray();

	// 显示消息内容
	virtual void Show() const = 0;
};

#endif

/*
消息的共有部分是消息代码和负载数据，可由微网协议或令牌协议承载传输。
都是二进制格式传输，所以有Read/Write等操作
都有Crc校验，用于判断消息是否被篡改过
*/
