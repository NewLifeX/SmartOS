#include "Time.h"
#include "HelloMessage.h"

// 请求：2版本 + S类型 + S名称 + 8本地时间 + 6本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8本地时间 + 6对方IP端口 + 1加密算法 + N密钥
// 错误：0xFE/0xFD + 1协议 + S服务器 + 2端口

// 初始化消息，各字段为0
HelloMessage::HelloMessage() : Ciphers(1), Key(0)
{
	Version		= Sys.Version;

	ushort code = _REV16(Sys.Code);
	ByteArray bs(&code, 2);
	Type		= bs.ToHex('\0');
	Name		= Sys.Company;
	LocalTime	= Time.Now().TotalMicroseconds();
	Ciphers[0]	= 1;

	Protocol	= 2;
	Port		= 0;
}

HelloMessage::HelloMessage(const HelloMessage& msg) : MessageBase(msg), Ciphers(1), Key(0)
{
	Version		= msg.Version;
	Type		= msg.Type;
	Name		= msg.Name;
	LocalTime	= msg.LocalTime;
	EndPoint	= msg.EndPoint;
	Ciphers		= msg.Ciphers;
	Key			= msg.Key;

	Protocol	= msg.Protocol;
	Port		= msg.Port;
	Server		= msg.Server;
}

// 从数据流中读取消息
bool HelloMessage::Read(Stream& ms)
{
	if(Reply && Error)
	{
		ErrCode	= ms.ReadByte();
		if(ErrCode == 0xFE || ErrCode == 0xFD)
		{
			Protocol	= ms.ReadByte();
			Server		= ms.ReadString();
			Port		= ms.ReadUInt16();
			VisitToken	= ms.ReadString();
			return false;
		}
		else if(ErrCode < 0x80)
		{
			ErrMsg		= ms.ReadString();
		}
		else
		{
			debug_printf("无法识别错误码 0x%02X \r\n", ErrCode);

			return false;
		}

		return true;
	}

	Version		= ms.ReadUInt16();
	Type		= ms.ReadString();
	Name		= ms.ReadString();
	LocalTime	= ms.ReadUInt64();
	EndPoint	= ms.ReadArray(6);

	if(!Reply)
	{
		Ciphers	= ms.ReadArray();
	}
	else
	{
		Ciphers[0]	= ms.ReadByte();
		// 读取数组前，先设置为0，避免实际长度小于数组长度
		Key.SetLength(0);
		Key		= ms.ReadArray();
	}

	return false;
}

// 把消息写入数据流中
void HelloMessage::Write(Stream& ms) const
{
	ms.Write(Version);
	ms.WriteArray(Type);
	ms.WriteArray(Name);
	ms.Write(LocalTime);
	ms.Write(EndPoint.ToArray());

	if(!Reply)
	{
		ms.WriteArray(Ciphers);
	}
	else
	{
		ms.Write(Ciphers[0]);
		ms.WriteArray(Key);
	}
}

#if DEBUG
// 显示消息内容
String& HelloMessage::ToStr(String& str) const
{
	str += "握手";
	if(Reply) str += "#";

	if(Reply && Error)
	{
		if(Protocol == 1)
			str = str + " TCP ";
		else if(Protocol == 2)
			str = str + " UDP ";

		str += Server;
		str = str + " " + Port;

		return str;
	}

	str.Append(" Ver=").Append(Version, 16, 4);
	str = str + " " + Type + " " + Name + " ";

	DateTime dt;
	dt.ParseUs(LocalTime);
	str += dt.ToString();

	str = str + " " + EndPoint;

	str = str + " Ciphers[" + Ciphers.Length() + "]=";
	for(int i=0; i<Ciphers.Length(); i++)
	{
		str.Append(Ciphers[i]).Append(' ');
	}

	if(Reply) str += " Key=" + Key;

	return str;
}
#endif
