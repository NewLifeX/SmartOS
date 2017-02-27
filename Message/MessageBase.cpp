#include "MessageBase.h"

// 初始化消息，各字段为0
MessageBase::MessageBase()
{
	Reply = false;
	Error = false;
}

MessageBase::MessageBase(const MessageBase& msg)
{
	Reply = msg.Reply;
	Error = msg.Error;
}

bool MessageBase::ReadMessage(const Message& msg)
{
	TS("MessageBase::ReadMessage");

	Reply = msg.Reply > 0;
	Error = msg.Error > 0;

	//Stream ms(msg.Data, msg.Length);
	auto ms = msg.ToStream();

	return Read(ms);
}

void MessageBase::WriteMessage(Message& msg) const
{
	TS("MessageBase::WriteMessage");

	// 如果是令牌消息，这里就要自己小心了
	//Stream ms(msg.Data, 256);
	//MemoryStream ms;
	auto ms = msg.ToStream();

	Write(ms);

	msg.Length = ms.Position();
	//msg.SetData(Buffer(ms.GetBuffer(), ms.Position()));
}
