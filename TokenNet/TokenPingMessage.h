#ifndef __TokenPingMessage_H__
#define __TokenPingMessage_H__

#include "Message\MessageBase.h"

class TokenPingMessage : public MessageBase
{
public:
	UInt64	LocalTime;	// 时间ms
	UInt64	ServerTime;	// 时间ms
	Buffer*	Data;

	TokenPingMessage();
	TokenPingMessage(const TokenPingMessage& msg);

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms) const;

	// 显示消息内容
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

#endif
