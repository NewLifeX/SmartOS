#include "DiscoverMessage.h"

// 初始化消息，各字段为0
DiscoverMessage::DiscoverMessage() : HardID(0x10)
{
}

// 从数据流中读取消息
bool DiscoverMessage::Read(Stream& ms)
{
	Type = ms.Read<ushort>();
	ms.ReadArray(HardID);

	return false;
}

// 把消息写入数据流中
void DiscoverMessage::Write(Stream& ms)
{
	ms.Write(Type);
	ms.WriteArray(HardID);
}

String& DiscoverMessage::ToStr(String& str) const
{
	str += "发现";
	if(Reply) str += "#";
	str.Format(" Type=0x%04X", Type);
	str += " HardID=" + HardID;

	return str;
}
