#ifndef __HelloMessage_H__
#define __HelloMessage_H__

#include "Message\MessageBase.h"
#include "Net\Net.h"

// 握手消息
// 请求：2版本 + S类型 + S名称 + 8本地时间 + 6本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8本地时间 + 6对方IP端口 + 1加密算法 + N密钥
// 错误：0xFE/0xFD + 1协议 + S服务器 + 2端口
class HelloMessage : public MessageBase
{
public:
	ushort		Version;	// 版本
	String		Type;		// 类型
	String		Name;		// 名称
	UInt64		LocalTime;	// 时间ms
	IPEndPoint	EndPoint;
	String		Cipher;

	ByteArray	Key;		// 密钥

	byte		ErrCode;	// 错误码
	String		ErrMsg;		// 错误信息

	byte		Protocol;	// 协议,17为UDP  6为TCP
	String		Server;		// 服务器地址。可能是域名或IP
	ushort		Port;		// 本地端口
	String		VisitToken;	//访问令牌 

	// 初始化消息，各字段为0
	HelloMessage();
	HelloMessage(const HelloMessage& msg);

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms) const;

	// 显示消息内容
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

class TokenPingMessage : public MessageBase
{
public:
	UInt64		TimeX;	// 时间ms

	TokenPingMessage();
	TokenPingMessage(const TokenPingMessage& msg);

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms) const;

	// 显示消息内容
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

#endif
