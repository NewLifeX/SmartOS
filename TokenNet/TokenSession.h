#ifndef __TokenSession_H__
#define __TokenSession_H__

#include "Sys.h"
#include "TokenNet\TokenConfig.h"
#include "TokenMessage.h"
#include "HelloMessage.h"

#include "TokenNet\TokenController.h"
#include "TokenNet\TokenClient.h"

#include "Message\DataStore.h"
#include "Message\BinaryPair.h"

// 令牌会话
class TokenSession
{
public:
	TokenClient&		Client;		// 客户端
	TokenController&	Control;	// 来源通道

	uint	Token;		// 令牌
	ByteArray	ID;		// 设备标识
	ByteArray	Key;	// 密钥
	String		Name;	// 名称
	ushort	Version;	// 版本
	IPEndPoint	Remote;	// 地址

	int		Status;		// 状态。0准备、1握手完成、2登录后
	UInt64	LoginTime;	// 登录时间ms
	UInt64	LastActive;	// 最后活跃时间ms

	TokenSession(TokenClient& client, TokenController& ctrl);
	~TokenSession();

	void OnReceive(TokenMessage& msg);

private:
	bool OnHello(TokenMessage& msg);
	bool OnLogin(TokenMessage& msg);
	bool OnPing(TokenMessage& msg);
};

#endif
