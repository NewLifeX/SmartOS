#include "Message.h"

// 初始化消息，各字段为0
Message::Message(byte code)
{
	Code	= code;
	Length	= 0;
	Data	= NULL;
}

Message::Message(Message& msg)
{
	Code = msg.Code;
	Length = msg.Length;
	if(Length) memcpy(Data, msg.Data, Length);
}

// 设置数据。
void Message::SetData(byte* buf, uint len)
{
	//assert_param(len <= ArrayLength(Data));

	Length = len;
	if(len > 0)
	{
		assert_ptr(buf);
		assert_ptr(Data);
		memcpy(Data, buf, len);
	}
}
