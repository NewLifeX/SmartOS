#include "Message.h"

// 初始化消息，各字段为0
Message::Message(byte code)
{
	Code	= code;
	Length	= 0;
	Data	= NULL;
	Reply	= false;
	Error	= false;
}

// 设置数据。
void Message::SetData(const byte* buf, uint len, uint offset)
{
	//assert_param(len <= ArrayLength(Data));

	Length = len;
	if(len > 0 && buf != Data + offset)
	{
		assert_ptr(buf);
		assert_ptr(Data);
		memcpy(Data + offset, buf, len);
	}
}

void Message::SetData(const ByteArray& bs, uint offset)
{
	Length = bs.Length();
	if(Length > 0 && bs.GetBuffer() != Data + offset) bs.CopyTo(Data + offset, Length);
}

bool Message::Clone(const Message& msg)
{
	Stream ms;
	msg.Write(ms);

	ms.SetPosition(0);

	return Read(ms);
}

/*// 负载数据转数据流
Stream Message::ToStream()
{
	return Stream(Data, Length);
}

// 负载数据转字节数组
ByteArray Message::ToArray()
{
	return ByteArray(Data, Length);
}*/
