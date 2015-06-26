#include "HelloMessage.h"

// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥

// 初始化消息，各字段为0
HelloMessage::HelloMessage() : Ciphers(1), Key(16)
{
	Version		= Sys.Version;

	ushort code = __REV16(Sys.Code);
	ByteArray bs((byte*)&code, 2);
	Type = bs.ToHex();
	Name.Set(Sys.Name);

	LocalTime	= Time.Current();
	Ciphers[0]	= 1;

	Reply		= false;
}

HelloMessage::HelloMessage(HelloMessage& msg) : Ciphers(1), Key(16)
{
	Version		= msg.Version;
	Type		= msg.Type;
	Name		= msg.Name;
	LocalTime	= msg.LocalTime;
	EndPoint	= msg.EndPoint;
	Ciphers		= msg.Ciphers;
	Key			= msg.Key;
}

// 从数据流中读取消息
bool HelloMessage::Read(Stream& ms)
{
	Version		= ms.Read<ushort>();

	ms.ReadString(Type.Clear());
	ms.ReadString(Name.Clear());

	LocalTime	= ms.Read<ulong>() / 10;

	EndPoint.Address = ms.ReadBytes(4);
	EndPoint.Port = ms.Read<ushort>();

	if(!Reply)
	{
		ms.ReadArray(Ciphers);
	}
	else
	{
		Ciphers[0]	= ms.Read<byte>();
		ms.ReadArray(Key);
	}

	return false;
}

// 把消息写入数据流中
void HelloMessage::Write(Stream& ms)
{
	ms.Write(Version);

	ms.WriteString(Type);
	ms.WriteString(Name);

	ms.Write(LocalTime * 10);

	ms.Write(EndPoint.Address.ToArray(), 0, 4);
	ms.Write((ushort)EndPoint.Port);

	ms.WriteArray(Ciphers);

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

bool HelloMessage::Read(Message& msg)
{
	Stream ms(msg.Data, msg.Length);
	return Read(ms);
}

void HelloMessage::Write(Message& msg)
{
	//Stream ms(msg.Data, ArrayLength(msg._Data));
	Stream ms(msg.Data, 256);

	Write(ms);

	msg.Length = ms.Position();
}

// 显示消息内容
void HelloMessage::Show()
{
	debug_printf("Ver=%d.%d Type=%s Name=%s ", Version >> 8, Version & 0xFF, Type.GetBuffer(), Name.GetBuffer());
	DateTime dt;
	dt.Parse(LocalTime / 10);
	debug_printf("%s ", dt.ToString());

	EndPoint.Show();

	debug_printf(" Ciphers[%d]=", Ciphers.Length());
	for(int i=0; i<Ciphers.Length(); i++)
	{
		debug_printf("%d ", Ciphers[i]);
	}
	debug_printf("\r\n");
}
