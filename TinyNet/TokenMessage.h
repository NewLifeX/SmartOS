#ifndef __TokenMessage_H__
#define __TokenMessage_H__

#include "Sys.h"
#include "Stream.h"

// 处理消息，返回是否成功
typedef bool (*TokenMessageHandler)(TokenMessage& msg, void* param);

// 令牌消息
class TokenMessage
{
private:

public:
	uint	Token;		// 令牌

	byte	Code:7;		// 功能码
	byte	Error:1;	// 是否异常

	byte	Length;		// 数据长度
	byte	Data[256];	// 数据

	ushort	Crc;		// 校验码
	ushort	Crc2;		// 动态计算得到的校验码

	TokenMessage();

	bool Read(MemoryStream& ms);
	void Write(MemoryStream& ms);

	uint Size() const;

	// 设置数据
	void SetData(byte* buf, uint len);
	void SetError(byte error);
private:
};

// 令牌控制器
class TokenController
{
private:
	ITransport* _port;

public:
	TokenController(ITransport* port);
	~TokenController();

	// 发送消息，传输口参数为空时向所有传输口发送消息
	bool Send(byte code, byte* buf = NULL, uint len = 0);
	// 发送消息，expire毫秒超时时间内，如果对方没有响应，会重复发送
	bool Send(TokenMessage& msg, int expire = -1);

	// 收到消息时触发
	TokenMessageHandler	Received;
	void*			Param;

};

#endif
