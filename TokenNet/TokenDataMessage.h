#ifndef __TokenDataMessage_H__
#define __TokenDataMessage_H__

#include "Sys.h"
#include "Stream.h"

#include "Message\Message.h"
#include "Message\MessageBase.h"

// 令牌消息
class TokenDataMessage : public MessageBase
{
public:
	byte ID;
	uint Start;
	uint Size;
	ByteArray Data;

	TokenDataMessage();
	TokenDataMessage(const TokenDataMessage& msg);

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
