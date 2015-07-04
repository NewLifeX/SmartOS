#include "Message.h"

// 初始化消息，各字段为0
Message::Message(byte code)
{
	Code	= code;
	Length	= 0;
	Data	= NULL;
	Reply	= 0;
}

/*Message::Message(Message& msg)
{
	Code	= msg.Code;
	Length	= msg.Length;
	Reply	= msg.Reply;

	// 基类构造函数先执行，子类来不及赋值Data，所以这里不要拷贝
	//assert_ptr(Data);
	//if(Length) memcpy(Data, msg.Data, Length);
}*/

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
