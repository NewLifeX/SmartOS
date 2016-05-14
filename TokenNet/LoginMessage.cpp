#include "LoginMessage.h"
#include "Security\MD5.h"
#include "Message\BinaryPair.h"

// 初始化消息，各字段为0
LoginMessage::LoginMessage() : Key(0)
{
}

// 从数据流中读取消息
bool LoginMessage::Read(Stream& ms)
{
	BinaryPair bp(ms);
	if (!Reply)
	{
		bp.Get("UserName", User);
		bp.Get("Password", Pass);
		bp.Get("Salt", Salt);
	}
	else if (!Error)
	{
		bp.Get("Token", Token);
		bp.Get("Password", Pass);
		bp.Get("Key",Key);
	}
    return false;
}

// 把消息写入数据流中
void LoginMessage::Write(Stream& ms) const
{
	BinaryPair bp(ms);

	if(!Reply)
	{
		bp.Set("UserName", User);
		bp.Set("Password", Pass);

		if(Salt.Length() > 0)
			bp.Set("Salt", Salt);
		else
		{
			UInt64 now = Sys.Ms();
			bp.Set("Salt", MD5::Hash(Buffer(&now, 8)));
		}
	}
	else if(!Error)
	{
		bp.Set("Token", Token);
		bp.Set("Key", Key);
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
