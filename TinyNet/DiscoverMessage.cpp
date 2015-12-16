#include "DiscoverMessage.h"

// 初始化消息，各字段为0
DiscoverMessage::DiscoverMessage() : HardID(0x10), Pass(0x08)
{
	Type		= Sys.Code;
	Version		= Sys.Version;
	PanID		= 0;
	SendMode	= 0;
	Channel		= 0;
}

// 从数据流中读取消息
bool DiscoverMessage::Read(Stream& ms)
{
	if(!Reply)
	{
		Type		= ms.ReadUInt16();
		HardID		= ms.ReadArray();
		Version		= ms.ReadUInt16();
		PanID		= ms.ReadUInt16();
		SendMode	= ms.ReadByte();
		Channel		= ms.ReadByte();
	}
	else
	{
		Address	= ms.ReadByte();
		Pass	= ms.ReadArray();
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
		ms.Write(Version);
		ms.Write(PanID);
		ms.Write(SendMode);		
		ms.Write(Channel);
	}
	else
	{
		ms.Write(Address);
		ms.WriteArray(Pass);
	}
}

#if DEBUG
String& DiscoverMessage::ToStr(String& str) const
{
	str += "发现";
	if(!Reply)
	{
		str.Append(" Type=").Append(Type, 16, 4);
		str = str + " HardID=" + HardID;
		str.Append(" Ver=").Append(Version, 16, 4);
		str = str + " PanID=" +.Append(PanID,16, 4);
		str = str + " SendMode=" + SendMode;
		str = str + " Channel=" + SendMode;
	}
	else
	{
		str += "#";
		str = str + " Address=" + Address;
		str = str + " Pass=" + Pass;
	}

	return str;
}
#endif
