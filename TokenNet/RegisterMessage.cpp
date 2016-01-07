#include "RegisterMessage.h"

#include "Security\MD5.h"

// 初始化消息，各字段为0
RegisterMessage::RegisterMessage() : HardID(16), Key(6)
{
}
// 从数据流中读取消息
bool RegisterMessage::Read(Stream& ms)
{
	if(!Reply)
	{	
		HardID = ms.ReadArray();
		Key    = ms.ReadArray();			
	}
	else
		if(!Error)
		{	
			HardID = ms.ReadArray();	
			Key   = ms.ReadArray();
		}
		
    return false;		
}

// 把消息写入数据流中
void RegisterMessage::Write(Stream& ms) const
{
	if(!Reply)
	{
		ms.WriteArray(HardID);
		// 密码取MD5后传输
		ms.WriteArray(MD5::Hash(Key));		
	}
	else
		if(!Error)
		{		
			//ms.WriteArray(Key);
		}		
}

#if DEBUG
// 显示消息内容
String& RegisterMessage::ToStr(String& str) const
{
	str += "注册";
	if(Reply) str += "#";
	str = str + " HardID=" + HardID + " Key=" + Key;

	return str;
}
#endif
