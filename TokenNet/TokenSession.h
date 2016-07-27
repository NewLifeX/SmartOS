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

#if DEBUG

class SessionStat : public Object
{
public:
	ushort OnHello;		// 握手次数
	ushort OnLogin;		// 登录次数
	ushort OnPing;		// 心跳次数
	static uint BraHello;	// 广播握手次数
	static uint DecError;	// 解密失败次数
public:
	SessionStat();
	~SessionStat();
	void Clear();

	virtual String& ToStr(String& str) const;
};

#endif 

// 令牌会话
class TokenSession :public Object
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

	int		Status;		// 状态。0准备、1握手完成、2登录后、3心跳中
	UInt64	LoginTime;	// 登录时间ms
	UInt64	LastActive;	// 最后活跃时间ms

#if DEBUG
	static uint		HisSsNum;		// 历史Session个数
	static uint StatShowTaskID;		// 输出统计信息的任务ID
	SessionStat Stat;				// 统计信息
#endif

	TokenSession(TokenClient& client, TokenController& ctrl);
	~TokenSession();

	bool Send(TokenMessage& msg);
	void OnReceive(TokenMessage& msg);
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
private:
	bool OnHello(TokenMessage& msg);
	bool OnLogin(TokenMessage& msg);
	bool OnPing(TokenMessage& msg);
};

#endif
