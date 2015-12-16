#include "JoinMessage.h"

// 初始化消息，各字段为0
JoinMessage::JoinMessage() : HardID(0x10), Password(0x08)
{
	Version	= 1;
	Kind	= Sys.Code;
	TranID	= 0;

	Server	= 0;
	PanID	= 0;
	SendMode= 0;
	Channel	= 0;
	Speed	= 0;
	Address	= 0;
}

// 从数据流中读取消息
bool JoinMessage::Read(Stream& ms)
{
	if(!Reply)
	{
		Version	= ms.ReadByte();
		Kind	= ms.ReadUInt16();
		TranID	= ms.ReadUInt32();
		HardID	= ms.ReadArray();
		Name	= ms.ReadString();
		//ms.ReadString().CopyTo(Name, ArrayLength(Name));
	
		
	}
	else
	{
		Server	= ms.ReadByte();
		WirKind	= ms.ReadByte();
		Channel	= ms.ReadByte();
		Speed	= ms.ReadByte();
		Address	= ms.ReadByte();
		Password= ms.ReadArray();
		TranID	= ms.ReadUInt32();
		HardID	= ms.ReadArray();
	}

	return true;
}

// 把消息写入数据流中
void JoinMessage::Write(Stream& ms)
{
	if(!Reply)
	{
		ms.Write(Version);
		ms.Write(Kind);
		ms.Write(TranID);
		ms.WriteArray(HardID);
		ms.Write(Name);
	}
	else
	{
		ms.Write(Server);
		ms.Write(WirKind);
		ms.Write(PanID);
		ms.Write(SendMode);
		ms.Write(Channel);
		ms.Write(Speed);
		ms.Write(Address);
		ms.WriteArray(Password);
		ms.Write(TranID);
		ms.WriteArray(HardID);
	}
}

#if DEBUG
String& JoinMessage::ToStr(String& str) const
{
	str += "组网";
	if(!Reply)
	{
		str = str + " Version=" + Version;
		str.Append(" Kind=").Append(Kind, 16, 4);
		str.Append(" TranID=").Append(TranID, 16, 8);
		str = str + " HardID=" + HardID;
	}
	else
	{
		str += "#";
		str = str + " Server=" + Server;
		str = str + " WirKind=" + WirKind;
		str.Append("PanID=").Append(PanID, 16, 4);
		str = str + " SendMode=" + SendMode;
		str = str + " Channel=" + Channel;
		str = str + " Speed=" + Speed;
		str = str + " Address=" + Address;
		str = str + " Password=" + Password;
		str.Append(" TranID=").Append(TranID, 16, 8);
		str = str + " HardID=" + HardID;
	}

	return str;
}
#endif
