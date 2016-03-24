#ifndef __TokenDevice_H__
#define __TokenDevice_H__

#include "Sys.h"
#include "TokenNet\TokenConfig.h"
#include "TokenMessage.h"
#include "HelloMessage.h"


// 微网客户端
class TokenDevice::TokenClient
{
private:

public:
	//TokenDevice();

	// void Open();		// 打开客户端
	// void Close();
    // 
	// // 发送消息
	// bool Send(TokenMessage& msg, Controller* ctrl = nullptr);
	// bool Reply(TokenMessage& msg, Controller* ctrl = nullptr);
	// bool OnReceive(TokenMessage& msg, Controller* ctrl);
    // 
	// // 收到功能消息时触发
	// MessageHandler	Received;
	// void*			Param;

// 本地网络支持
public:

// 常用系统级消息
public:

};

#endif
