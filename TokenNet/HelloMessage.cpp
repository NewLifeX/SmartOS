#include "HelloMessage.h"

// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥

// 初始化消息，各字段为0
HelloMessage::HelloMessage() : Ciphers(1), Key(16)
{
	Version	= Sys.Version;
	ByteArray bs((byte*)&Sys.Code, 2);
	bs.ToHex(Type.Clear());
	Name.Set(Sys.Name);
	LocalTime = Time.Current() * 10;
	Ciphers[0] = 1;
}

// 从数据流中读取消息
bool HelloMessage::Read(Stream& ms)
{
	Version = ms.Read<ushort>();

	ms.ReadString(Type);
	ms.ReadString(Name);

	LocalTime = ms.Read<ulong>();

	EndPoint.Address = ms.ReadBytes(4);
	EndPoint.Port = ms.Read<ushort>();

	Ciphers[0] = ms.Read<byte>();

	return false;
}

// 把消息写入数据流中
void HelloMessage::Write(Stream& ms)
{
	ms.Write(Version);

	ms.WriteString(Type);
	ms.WriteString(Name);

	ms.Write(LocalTime);

	ms.Write(EndPoint.Address.ToArray(), 0, 4);
	ms.Write((ushort)EndPoint.Port);

	ms.WriteArray(Ciphers);
}

// 显示消息内容
void HelloMessage::Show()
{

}
