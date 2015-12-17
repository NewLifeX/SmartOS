#include "MessageBase.h"

// 初始化消息，各字段为0
MessageBase::MessageBase()
{
	Reply	= false;
}

bool MessageBase::ReadMessage(const Message& msg)
{
	TS("MessageBase::ReadMessage");

	Reply = msg.Reply;

	Stream ms(msg.Data, msg.Length);
	return Read(ms);
}

void MessageBase::WriteMessage(Message& msg) const
{
	TS("MessageBase::WriteMessage");

	// 如果是令牌消息，这里就要自己小心了
	//Stream ms(msg.Data, 256);
	MemoryStream ms;

	Write(ms);

	//msg.Length = ms.Position();
	msg.SetData(Array(ms.GetBuffer(), ms.Position()));
}
