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
		User	= ms.ReadString();
		Pass	= ms.ReadString();
		Salt	= ms.ReadArray();
	}
	else if(!Error)
	{
		Token	= ms.ReadUInt32();
		Pass	= ms.ReadString();
	}

    return false;
}

// 把消息写入数据流中
void LoginMessage::Write(Stream& ms) const
{
	if(!Reply)
	{
		ms.WriteString(User);
		ms.WriteString(Pass);

		if(Salt.Length() > 0)
			ms.WriteArray(Salt);
		else
		{
			UInt64 now = Sys.Ms();
			ms.WriteArray(MD5::Hash(Buffer(&now, 8)));
		}
	}
	else if(!Error)
	{
		ms.Write(Token);
		ms.WriteString(Pass);
	}
}

#if DEBUG
// 显示消息内容
String& LoginMessage::ToStr(String& str) const
{
	str += "登录";
	if(Reply) str += "#";
	str = str + " User=" + User + " Pass=" + Pass + " Salt=" + Salt;

	return str;
}
#endif
