#ifndef __TokenSession_H__
#define __TokenSession_H__

#include "Kernel\Sys.h"
#include "TokenNet\TokenConfig.h"
#include "TokenMessage.h"
#include "HelloMessage.h"

#include "TokenNet\TokenController.h"
#include "TokenNet\TokenClient.h"

#include "Message\DataStore.h"

// 令牌会话
class TokenSession
{
public:
	TokenClient&		Client;		// 客户端
	TokenController&	Control;	// 来源通道

	uint	Token;		// 令牌
	ByteArray	Key;	// 密钥
	String	DeviceID;	// 名称
	ushort	Version;	// 版本
	IPEndPoint	Remote;	// 地址

	int		Status;		// 状态。0准备、1握手完成、2登录后、3心跳中
	UInt64	LoginTime;	// 登录时间ms
	UInt64	LastActive;	// 最后活跃时间ms

	TokenSession(TokenClient& client, TokenController& ctrl);
	~TokenSession();

	bool Send(TokenMessage& msg);
	void OnReceive(TokenMessage& msg);

	bool CheckExpired();

	String& ToStr(String& str) const;

	static void Show(IList& sessions);

private:
	bool OnHello(TokenMessage& msg);
	bool OnLogin(TokenMessage& msg);
	bool OnPing(TokenMessage& msg);
};

#endif
