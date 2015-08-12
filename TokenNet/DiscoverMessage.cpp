#include "DiscoverMessage.h"

// 初始化消息，各字段为0
DiscoverMessage::DiscoverMessage() : HardID(0x10), Pass(0x08)
{
	Type	= Sys.Code;
	Version	= Sys.Version;
	Switchs	= 0;
	Analogs	= 0;
}

// 从数据流中读取消息
bool DiscoverMessage::Read(Stream& ms)
{
	if(!Reply)
	{
		Type = ms.Read<ushort>();

		// 兼容旧版本，固定20字节的ID
		if(ms.Remain() == 20)
		{
			HardID.SetLength(20);
			ms.Read(HardID);
		}
		else
		{
			ms.ReadArray(HardID);
			
			if(ms.Remain() > 0)
			{
				Version	= ms.Read<ushort>();
				Switchs	= ms.Read<byte>();
				Analogs	= ms.Read<byte>();
			}
		}
	}
	else
	{
		ID = ms.Read<byte>();

		// 兼容旧版本，固定8字节密码
		if(ms.Remain() == 8)
		{
			Pass.SetLength(8);
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
		ms.Write(Version);
		ms.Write(Switchs);
		ms.Write(Analogs);
	}
	else
	{
		ms.Write(ID);
		ms.WriteArray(Pass);
	}
}

#if DEBUG
String& DiscoverMessage::ToStr(String& str) const
{
	str += "发现";
	if(!Reply)
	{
		str = str + " Type=" + (byte)(Type >> 8) + (byte)(Type & 0xFF);
		str = str + " HardID=" + HardID;
		str = str + " Version=" + (Version >> 8) + "." + (Version & 0xFF);
		str = str + " Switchs=" + Switchs;
		str = str + " Analogs=" + Analogs;
	}
	else
	{
		str += "#";
		str = str + " ID=" + ID;
		str = str + " Pass=" + Pass;
	}

	return str;
}
#endif
