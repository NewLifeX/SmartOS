#ifndef __TokenClient_H__
#define __TokenClient_H__

#include "Sys.h"
#include "Message.h"
#include "Controller.h"
#include "TokenMessage.h"
#include "..\TokenNet\HelloMessage.h"

#include "..\TinyIP\Udp.h"

// 微网客户端
class TokenClient
{
private:

public:
	uint	Token;		// 令牌
	byte	ID[16];		// 设备标识
	byte	Key[16];	// 通讯密码

	ulong	LoginTime;	// 登录时间
	ulong	LastActive;	// 最后活跃时间

	TokenController* Control;
	UdpSocket*	Udp;	// 用于广播握手消息的UDP
	
	TokenClient();

	void Open();		// 打开客户端
	
	// 发送消息
	void Send(Message& msg);
	void Reply(Message& msg);
	
	// 收到功能消息时触发
	MessageHandler	Received;
	void*			Param;

// 常用系统级消息
public:
	// 握手广播
	HelloMessage	Hello;
	void SayHello(bool broadcast = false, int port = 0);
	MessageHandler OnHello;
	static bool SayHello(Message& msg, void* param);

	/*// Ping指令用于保持与对方的活动状态
	void Ping();
	MessageHandler OnPing;
	static bool Ping(Message& msg, void* param);

	// 询问及设置系统时间
	static bool SysTime(Message& msg, void* param);

	// 询问系统标识号
	static bool SysID(Message& msg, void* param);

	// 设置系统模式
	static bool SysMode(Message& msg, void* param);*/
	
// 通用用户级消息
public:
	byte*	Switchs;// 开关指针
	int*	Regs;	// 寄存器指针
};

#endif
