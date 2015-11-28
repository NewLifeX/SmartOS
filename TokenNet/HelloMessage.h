#ifndef __HelloMessage_H__
#define __HelloMessage_H__

#include "Message\MessageBase.h"
#include "Net\Net.h"

// 握手消息
// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + 1加密算法 + N密钥
class HelloMessage : public MessageBase
{
public:
	ushort		Version;	// 版本
	String		Type;		// 类型
	String		Name;		// 名称
	ulong		LocalTime;	// 时间ms
	IPEndPoint	EndPoint;
	ByteArray	Ciphers;

	ByteArray	Key;		// 密钥

	byte		ErrCode;	// 错误码

	byte	Protocol;		// 协议，UDP=0/TCP=1
	ushort	Port;			// 本地端口

	uint	ServerIP;		// 服务器IP地址。服务器域名解析成功后覆盖
	ushort	ServerPort;		// 服务器端口
	char	Server[32];		// 服务器域名。出厂为空，从厂商服务器覆盖，恢复出厂设置时清空

	// 初始化消息，各字段为0
	HelloMessage();
	HelloMessage(HelloMessage& msg);

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms);

	// 显示消息内容
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

#endif
