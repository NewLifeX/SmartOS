#include "HelloMessage.h"

// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥

// 初始化消息，各字段为0
HelloMessage::HelloMessage(byte code)
{
	Code	= code;
	Length	= 0;
	Data	= NULL;
	Reply	= 0;
}

HelloMessage::HelloMessage(HelloMessage& msg)
{
	Code	= msg.Code;
	Length	= msg.Length;
	Reply	= msg.Reply;

	// 基类构造函数先执行，子类来不及赋值Data，所以这里不要拷贝
	//assert_ptr(Data);
	//if(Length) memcpy(Data, msg.Data, Length);
}

// 设置数据。
void HelloMessage::SetData(byte* buf, uint len)
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
