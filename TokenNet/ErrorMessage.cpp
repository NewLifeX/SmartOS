#include "ErrorMessage.h"
#include "Message\BinaryPair.h"

// 初始化消息，各字段为0
ErrorMessage::ErrorMessage() : ErrorContent("")
{
	ErrorCode	= 0;
}

// 读取消息
bool ErrorMessage::Read(const Message& msg)
{
	auto ms	= msg.ToStream();
	BinaryPair bp(ms);

	bp.Get("ErrorCode", ErrorCode);
	bp.Get("ErrorMessage", ErrorContent);

	return true;
}

// 写入消息
void ErrorMessage::ErrorMessage::Write(Message& msg) const
{
	auto ms	= msg.ToStream();
	BinaryPair bp(ms);

	bp.Set("ErrorCode", ErrorCode);
	bp.Set("ErrorMessage", ErrorContent);
}
