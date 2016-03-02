#ifndef __LoginMessage_H__
#define __LoginMessage_H__

#include "Message\MessageBase.h"
#include "Net\Net.h"

// 登录消息
class LoginMessage : public MessageBase
{
public:
	String		User;	// 登录名
	String		Pass;	// 登录密码
	ByteArray	Salt;	// 加盐

	uint		Token;	//令牌

	// 初始化消息，各字段为0
	LoginMessage();

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
