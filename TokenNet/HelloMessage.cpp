#include "HelloMessage.h"

// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥

// 初始化消息，各字段为0
HelloMessage::HelloMessage() : Ciphers(16), Key(16)
{
	Version	= Sys.Version;
	ByteArray bs((byte*)&Sys.Code, 2);
	bs.ToHex(Type);
	Name.Set(Sys.Name);
}

// 从数据流中读取消息
bool HelloMessage::Read(Stream& ms)
{
	return false;
}

// 把消息写入数据流中
void HelloMessage::Write(Stream& ms)
{
	
}

// 显示消息内容
void HelloMessage::Show()
{
	
}
