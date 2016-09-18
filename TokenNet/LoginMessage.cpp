#include "LoginMessage.h"
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
		// bp.Get("VisitToken", VisitToken);
	}
	else if (!Error)
	{
		bp.Get("Token", Token);
		//bp.Get("Password", Pass);
		bp.Get("Key", Key);
	}
	else
	{
		bp.Get("ErrorCode", ErrorCode);
		bp.Get("ErrorMessage", ErrorMessage);
	}
	return false;
}

// 把消息写入数据流中
void LoginMessage::Write(Stream& ms) const
{
	BinaryPair bp(ms);

	if (!Reply)
	{
		bp.Set("UserName", User);
		bp.Set("Password", Pass);
		if(Cookie.Length())	bp.Set("Cookie", Cookie);
		if(Salt.Length())	bp.Set("Salt", Salt);
	}
	else if (!Error)
	{
		bp.Set("Token", Token);
		if(Key.Length())	bp.Set("Key", Key);
	}
}

#if DEBUG
// 显示消息内容
String& LoginMessage::ToStr(String& str) const
{
	str += "登录";
	if (Reply) str += "#";

	if (Error)
	{
		str = str + " ErrorCode=" + ErrorCode + " ErrorMessage=" + ErrorMessage;

		return str;
	}
	
	if(!Reply)
	{
		str = str + " User=" + User + " Pass=" + Pass + " Salt=" + Salt;
	}
	else
	{
		str += " Token=";
		str.Concat(Token, 16);
		str = str + " Key=" + Key;
	}
	if (Cookie.Length()) str = str + "  Cookie[" + Cookie.Length() + "]";

	return str;
}
#endif
