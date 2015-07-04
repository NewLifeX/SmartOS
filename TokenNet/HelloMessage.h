#ifndef __HelloMessage_H__
#define __HelloMessage_H__

#include "Sys.h"
#include "Stream.h"
#include "TinyNet\Message.h"

// 握手消息
// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥
class HelloMessage : public Object
{
public:
	ushort		Version;	// 版本
	String		Type;		// 类型
	String		Name;		// 名称
	ulong		LocalTime;	// 时间
	IPEndPoint	EndPoint;
	ByteArray	Ciphers;
	ByteArray	Key;		// 密钥

	bool		Reply;		// 是否响应
	
	// 初始化消息，各字段为0
	HelloMessage();
	HelloMessage(HelloMessage& msg);

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms) const;

	virtual bool Read(Message& msg);
	virtual void Write(Message& msg) const;

	// 显示消息内容
	virtual String& ToStr(String& str) const;
};

#endif
