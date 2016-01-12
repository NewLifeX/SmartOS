#include "Time.h"
#include "HelloMessage.h"

// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥

// 初始化消息，各字段为0
HelloMessage::HelloMessage() : Ciphers(1), Key(0)
{
	Version		= Sys.Version;

	ushort code = __REV16(Sys.Code);
	ByteArray bs(&code, 2);
	bs.Show(true);
	Type		= bs.ToHex('\0');
	debug_printf("HelloMessage::Load 从配置区加载配置%s\r\n",Type);
	Name		= Sys.Company;
	LocalTime	= Time.Now().TotalMicroseconds();
	Ciphers[0]	= 1;
	Protocol	= 2;
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
	Version		= ms.ReadUInt16();
	Type		= ms.ReadString();
	Name		= ms.ReadString();

	LocalTime	= ms.ReadUInt64();

	//EndPoint.Address	= ms.ReadBytes(4);
	//EndPoint.Port		= ms.ReadUInt16();
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
	if(Name.Length() != 0)
		ms.WriteArray(Name);
	else
		ms.WriteArray(String(Sys.Company));

	ms.Write(LocalTime);

	//ms.Write(EndPoint.Address.ToArray());
	//ms.Write((ushort)EndPoint.Port);
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
