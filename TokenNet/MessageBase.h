#ifndef __MessageBase_H__
#define __MessageBase_H__

#include "Sys.h"
#include "Stream.h"
#include "TinyNet\Message.h"

// 应用消息基类
class MessageBase : public Object
{
public:
	bool		Reply;	// 是否响应

	// 初始化消息，各字段为0
	MessageBase();

	// 从数据流中读取消息
	virtual bool Read(Stream& ms) = 0;
	// 把消息写入数据流中
	virtual void Write(Stream& ms) = 0;

	virtual bool ReadMessage(Message& msg);
	virtual void WriteMessage(Message& msg);
};

#endif
