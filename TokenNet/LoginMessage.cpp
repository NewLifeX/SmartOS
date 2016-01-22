#include "LoginMessage.h"

#include "Security\MD5.h"

// 初始化消息，各字段为0
LoginMessage::LoginMessage()
{
}

// 从数据流中读取消息
bool LoginMessage::Read(Stream& ms)
{
	if(!Reply)
	{
		Name	= ms.ReadString();
		Key		= ms.ReadString();
		Salt	= ms.ReadArray();
	}
	else if(!Error)
	{
		Token	= ms.ReadUInt32();
		Key		= ms.ReadString();
	}

    return false;
}

// 把消息写入数据流中
void LoginMessage::Write(Stream& ms) const
{
	if(!Reply)
	{
		ms.WriteArray(Name);
		ms.WriteArray(MD5::Hash(Key));

		if(Salt.Length() > 0)
			ms.WriteArray(Salt);
		else
		{
			ulong now = Sys.Ms();
			ms.WriteArray(MD5::Hash(Array(&now, 8)));
		}
	}
	else if(!Error)
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
	str = str + " Name=" + Name + " Key=" + Key + " Salt=" + Salt;

	return str;
}
#endif
