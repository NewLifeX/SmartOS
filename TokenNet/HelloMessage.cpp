#include "Time.h"
#include "HelloMessage.h"

// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥

// 初始化消息，各字段为0
HelloMessage::HelloMessage() : Ciphers(1), Key(0)
{
	Version		= Sys.Version;

	ushort code = __REV16(Sys.Code);
	ByteArray bs((byte*)&code, 2);
	Type = bs.ToHex('\0');
	//Name.Set(Sys.Company); 	// Sys.company 是一个字符串   在flash里面   Name.Clear() 会出错
	Name		= Sys.Company;
	LocalTime	= Time.Current();
	Ciphers[0]	= 1;
}

HelloMessage::HelloMessage(HelloMessage& msg) : Ciphers(1), Key(0)
{
	Version		= msg.Version;
	Type		= msg.Type;
	Name		= msg.Name;
	LocalTime	= msg.LocalTime;
	EndPoint	= msg.EndPoint;
	Ciphers		= msg.Ciphers;
	Key			= msg.Key;
	Reply		= msg.Reply;
}

// 从数据流中读取消息
bool HelloMessage::Read(Stream& ms)
{
	Version		= ms.Read<ushort>();

	ms.ReadString(Type.Clear());
	ms.ReadString(Name.Clear());

	LocalTime	= ms.Read<ulong>();

	EndPoint.Address = ms.ReadBytes(4);
	EndPoint.Port = ms.Read<ushort>();

	if(!Reply)
	{
		ms.ReadArray(Ciphers);
	}
	else
	{
		Ciphers[0]	= ms.Read<byte>();
		// 读取数组前，先设置为0，避免实际长度小于数组长度
		Key.SetLength(0);
		ms.ReadArray(Key);
	}

	return false;
}

// 把消息写入数据流中
void HelloMessage::Write(Stream& ms)
{
	ms.Write(Version);

	ms.WriteString(Type);
	if(Name.Length() != 0)
		ms.WriteString(Name);
	else
	{
		String _name(Sys.Company);
		ms.WriteString(_name);
	}

	ms.Write(LocalTime);

	ms.Write(EndPoint.Address.ToArray());
	ms.Write((ushort)EndPoint.Port);

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
	str.Append(" Ver=").Append(Version, 16, 4);
	str = str + " " + Type + " " + Name + " ";
	DateTime dt;
	dt.Parse(LocalTime);
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
