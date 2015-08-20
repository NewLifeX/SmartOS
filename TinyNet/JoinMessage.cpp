#include "JoinMessage.h"

// 初始化消息，各字段为0
JoinMessage::JoinMessage() : HardID(0x10), Password(0x08)
{
	HardVer	= Sys.Version >> 8;
	SoftVer	= Sys.Version & 0xFF;
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
		HardVer	= ms.Read<byte>();
		SoftVer	= ms.Read<byte>();
		Kind	= ms.Read<ushort>();
		HardID	= ms.ReadArray();
		TranID	= ms.Read<ushort>();
	}
	else
	{
		Server	= ms.Read<byte>();
		Channel	= ms.Read<byte>();
		Speed	= ms.Read<byte>();
		Address	= ms.Read<byte>();
		Password	= ms.ReadArray();
	}

	return true;
}

// 把消息写入数据流中
void JoinMessage::Write(Stream& ms)
{
	if(!Reply)
	{
		ms.Write(HardVer);
		ms.Write(SoftVer);
		ms.Write(Kind);
		ms.WriteArray(HardID);
		ms.Write(TranID);
	}
	else
	{
		ms.Write(Server);
		ms.Write(Channel);
		ms.Write(Speed);
		ms.Write(Address);
		ms.WriteArray(Password);
	}
}

#if DEBUG
String& JoinMessage::ToStr(String& str) const
{
	str += "组网";
	if(!Reply)
	{
		str = str + " HardVer=" + HardVer;
		str = str + " SoftVer=" + SoftVer;
		str.Append(" Kind=").Append(Kind, 16, 4);
		str = str + " HardID=" + HardID;
		str.Append(" TranID=").Append(TranID, 16, 4);
	}
	else
	{
		str += "#";
		str = str + " Server=" + Server;
		str = str + " Channel=" + Channel;
		str = str + " Speed=" + Speed;
		str = str + " Address=" + Address;
		str = str + " Password=" + Password;
	}

	return str;
}
#endif
