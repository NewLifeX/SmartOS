#include "RegisterMessage.h"

#include "Security\MD5.h"

// 初始化消息，各字段为0
RegisterMessage::RegisterMessage() : Name(16), Pass(16)
{
}
// 从数据流中读取消息
bool RegisterMessage::Read(Stream& ms)
{
	if(!Reply)
	{	
		Name = ms.ReadString();
		Pass = ms.ReadString();			
	}
	//else
	//	if(!Error)
	//	{	
	//		Name = ms.ReadArray();	
	//		Pass   = ms.ReadArray();
	//	}
	//	
    return false;		
}

// 把消息写入数据流中
void RegisterMessage::Write(Stream& ms) const
{
	if(!Reply)
	{
		ms.WriteArray(Name);
		// 密码取MD5后传输
		ms.WriteArray(MD5::Hash(Pass));	
		ms.WriteArray(MD5::Hash(Name));
	}		
}

#if DEBUG
// 显示消息内容
String& RegisterMessage::ToStr(String& str) const
{
	str += "注册";
	if(Reply) str += "#";
	str = str + " Name=" + Name + " Pass=" + Pass;

	return str;
}
#endif
