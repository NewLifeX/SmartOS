#include "LoginMessage.h"

#include "Security\MD5.h"

// 初始化消息，各字段为0
LoginMessage::LoginMessage() : HardID(16), Key(6)
{
}

// 从数据流中读取消息
bool LoginMessage::Read(Stream& ms)
{
	ms.ReadArray(HardID);
	ms.ReadArray(Key);
	ms.ReadArray(Salt);

	Local.Address = ms.ReadBytes(4);
	Local.Port = ms.Read<ushort>();

	return false;
}

// 把消息写入数据流中
void LoginMessage::Write(Stream& ms)
{
	ms.WriteArray(HardID);

	// 密码取MD5后传输
	ByteArray bs;
	MD5::Hash(Key, bs);
	ms.WriteArray(bs);

	ulong now = Time.Current();
	Salt.Set((byte*)&now, 8);
	ms.WriteArray(Salt);

	ms.Write(Local.Address.ToArray(), 0, 4);
	ms.Write((ushort)Local.Port);
}

bool LoginMessage::Read(Message& msg)
{
	Stream ms(msg.Data, msg.Length);
	return Read(ms);
}

void LoginMessage::Write(Message& msg)
{
	Stream ms(msg.Data, 256);

	Write(ms);

	msg.Length = ms.Position();
}

// 显示消息内容
void LoginMessage::Show()
{
	debug_printf("登录");
	if(Reply) debug_printf("#");
	debug_printf(" HardID=");
	HardID.Show();
	debug_printf(" Key=");
	Key.Show();
	debug_printf(" Salt=");
	Salt.Show();

	debug_printf(" ");
	Local.Show();
}
