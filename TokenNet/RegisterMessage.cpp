#include "RegisterMessage.h"

#include "Security\MD5.h"

// 初始化消息，各字段为0
RegisterMessage::RegisterMessage() : User(), Pass(), Salt(0)
{
}

// 从数据流中读取消息
bool RegisterMessage::Read(Stream& ms)
{
	if(!Error)
	{
		User	= ms.ReadString();
		Pass	= ms.ReadString();
		Salt	= ms.ReadArray();
	}

    return false;
}

// 把消息写入数据流中
void RegisterMessage::Write(Stream& ms) const
{
	BinaryPair bp(ms);

	bp.Set("User", User);
	bp.Set("Pass", Pass);
	
	if(Salt.Length() > 0)
		bp.Set("Salt", Salt);
	else
	{
		UInt64 now = Sys.Ms();
		bp.Set("Salt", MD5::Hash(Buffer(&now, 8)));
	}
}

#if DEBUG
// 显示消息内容
String& RegisterMessage::ToStr(String& str) const
{
	str += "注册";
	if(Reply) str += "#";
	str = str + " User=" + User;
	str = str + " Pass=" + Pass;
	str = str + " Salt=";
	Salt.ToHex(str);

	return str;
}
#endif
