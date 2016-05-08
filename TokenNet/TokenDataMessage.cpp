
#include "TokenDataMessage.h"
#include "Message\BinaryPair.h"

TokenDataMessage::TokenDataMessage() :Data(0)
{
	ID = 0;
	Start = 0;
	Size = 0;
}

TokenDataMessage::TokenDataMessage(const TokenDataMessage& msg)
{
	ID = msg.ID;
	Start = msg.Start;
	Size = msg.Size;
	Data = msg.Data;
}

// 从数据流中读取消息
bool TokenDataMessage::Read(Stream& ms)
{
	BinaryPair bp(ms);
	bp.Get("ID", ID);
	bp.Get("Start", Start);
	bp.Get("Size", Size);
	bp.Get("MemoryData", Data);
	return true;
}

// 把消息写入数据流中
void TokenDataMessage::Write(Stream& ms) const
{
	BinaryPair bp(ms);
	bp.Set("ID", ID);
	bp.Set("Start", Start);
	bp.Set("Size", Size);
	bp.Set("MemoryData", Data);
}

#if DEBUG
// 显示消息内容
String& TokenDataMessage::ToStr(String& str) const
{
	str += "DateMsg";
	if (Reply) str += '#';
	int len = Data.Length();
	str = str + " ID:" + ID + " Start:" + Start + " Size:" + Size + " DataLen" + len;

	return str;
}
#endif
