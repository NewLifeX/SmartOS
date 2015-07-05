#include "DiscoverMessage.h"

// 初始化消息，各字段为0
DiscoverMessage::DiscoverMessage() : HardID(0x10), Pass(0x08)
{
}

// 从数据流中读取消息
bool DiscoverMessage::Read(Stream& ms)
{
	if(!Reply)
	{
		Type = ms.Read<ushort>();
		
		// 兼容旧版本，固定20字节的ID
		if(ms.Remain() == 20 && ms.Peek() != ms.Remain() - 1)
		{
			HardID.SetLength(20);
			ms.Read(HardID);
		}
		else
			ms.ReadArray(HardID);
	}
	else
	{
		ID = ms.Read<byte>();
		
		// 兼容旧版本，固定8字节密码
		if(ms.Remain() == 8 && ms.Peek() != ms.Remain() - 1)
		{
			HardID.SetLength(20);
			ms.Read(Pass);
		}
		else
			ms.ReadArray(Pass);
	}

	return true;
}

// 把消息写入数据流中
void DiscoverMessage::Write(Stream& ms)
{
	if(!Reply)
	{
		ms.Write(Type);
		ms.WriteArray(HardID);
	}
	else
	{
		ms.Write(ID);
		ms.WriteArray(Pass);
	}
}

String& DiscoverMessage::ToStr(String& str) const
{
	str += "发现";
	if(Reply)
	{
		str += "#";
		str.Format(" Type=0x%04X", Type);
		str += " HardID=" + HardID;
	}
	else
	{
		
	}

	return str;
}
