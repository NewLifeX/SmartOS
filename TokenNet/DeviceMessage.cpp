#include "DeviceMessage.h"

#include "Security\MD5.h"

// 初始化消息，各字段为0
DeviceMessage::DeviceMessage() : HardID(16), Key(6)
{
}

// 从数据流中读取消息
bool DeviceMessage::Read(Stream& ms)
{
	ms.ReadArray(HardID);
	ms.ReadArray(Key);
	ms.ReadArray(Salt);

	Local.Address = ms.ReadBytes(4);
	Local.Port = ms.ReadUInt16();

	return false;
}

// 把消息写入数据流中
void DeviceMessage::Write(Stream& ms) const
{
	ms.WriteArray(HardID);

	// 密码取MD5后传输
	ms.WriteArray(MD5::Hash(Key));

	ulong now = Sys.Ms();
	//Salt.Set((byte*)&now, 8);
	//ms.WriteArray(Salt);
	ms.WriteArray(Array(&now, 8));

	ms.Write(Local.Address.ToArray());
	ms.Write((ushort)Local.Port);
}

#if DEBUG
// 显示消息内容
String& DeviceMessage::ToStr(String& str) const
{
	str += "登录";
	if(Reply) str += "#";
	str = str + " HardID=" + HardID + " Key=" + Key + " Salt=" + Salt + " " + Local;

	return str;
}
#endif
