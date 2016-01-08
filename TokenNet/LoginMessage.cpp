#include "LoginMessage.h"

#include "Security\MD5.h"

// 初始化消息，各字段为0
LoginMessage::LoginMessage() : Name(16), Key(6)
{
	Reply = false;
}

// 从数据流中读取消息
bool LoginMessage::Read(Stream& ms)
{
	if(!Reply)
	{	
		Name = ms.ReadArray();
		Key    = ms.ReadArray();
		Salt   = ms.ReadArray();
	
		Local.Address = ms.ReadBytes(4);
		Local.Port = ms.ReadUInt16();		
	}
	else
		if(!Error)
		{
			Token = ms.ReadUInt32();
			Key   = ms.ReadArray();
		}
		
    return false;		
}

// 把消息写入数据流中
void LoginMessage::Write(Stream& ms) const
{
	if(!Reply)
	{
		ms.WriteArray(Name);
		// 密码取MD5后传输
		ms.WriteArray(MD5::Hash(Key));
		ulong now = Sys.Ms();
		//Salt.Set((byte*)&now, 8);
		//ms.WriteArray(Salt);
		ms.WriteArray(Array(&now, 8));
		ms.Write(Local.Address.ToArray());
		ms.Write((ushort)Local.Port);
	}
	else
		if(!Error)
		{
			ms.Write(Token);
			ms.WriteArray(Key);
		}		
}

#if DEBUG
// 显示消息内容
String& LoginMessage::ToStr(String& str) const
{
	str += "登录";
	if(Reply) str += "#";
	str = str + " Name=" + Name + " Key=" + Key + " Salt=" + Salt + " " + Local;

	return str;
}
#endif
