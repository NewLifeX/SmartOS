#ifndef __ErrorMessage_H__
#define __ErrorMessage_H__

#include "Sys.h"
#include "Message\Message.h"

// 错误消息
class ErrorMessage
{
public:
	byte	ErrorCode;		// 错误码
	String	ErrorContent;	// 错误内容

	// 初始化消息
	ErrorMessage();

	// 读取消息
	bool Read(const Message& msg);
	// 写入消息
	void Write(Message& msg) const;
};

#endif
