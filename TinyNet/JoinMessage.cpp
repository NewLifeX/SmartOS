#include "JoinMessage.h"

// 初始化消息，各字段为0
JoinMessage::JoinMessage() : HardID(0x10), Password(0x08)
{
	Version	= 1;
	Kind	= Sys.Code;
	TranID	= 0;

	Server	= 0;
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
	}
	else
	{
		Server	= ms.ReadByte();
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
void JoinMessage::Write(Stream& ms) const
{
	if(!Reply)
	{
		ms.Write(Version);
		ms.Write(Kind);
		ms.Write(TranID);
		ms.WriteArray(HardID);
	}
	else
	{
		ms.Write(Server);
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
		//str += " Kind=";
		str.Format(" Kind=%04X", Kind);
		//str += " TranID=";
		str.Format(" TranID=%08X", TranID);
		str = str + " HardID=" + HardID;
	}
	else
	{
		str += "#";
		str += " Server=" + Server;
		str += " Channel=" + Channel;
		str += " Speed=" + Speed;
		str += " Address=" + Address;
		str = str + " Password=" + Password;
		str.Format(" TranID=%08X", TranID);
		str = str + " HardID=" + HardID;
	}

	return str;
}
#endif
