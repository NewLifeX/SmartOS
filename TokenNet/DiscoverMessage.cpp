#include "DiscoverMessage.h"

// 初始化消息，各字段为0
DiscoverMessage::DiscoverMessage() : HardID(0x10)
{
}

// 从数据流中读取消息
bool DiscoverMessage::Read(Stream& ms)
{
	Type = ms.Read<ushort>();
	
	// 兼容旧版本，固定20字节的ID
	if(ms.Remain() == 20 && ms.Peek() != ms.Remain() - 1)
	{
		ms.Read(HardID.GetBuffer(), 0, HardID.Length());
		return true;
	}
	
	ms.ReadArray(HardID);

	return true;
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
