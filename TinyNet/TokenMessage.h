#ifndef __TokenMessage_H__
#define __TokenMessage_H__

#include "Sys.h"
#include "Stream.h"
#include "Net\ITransport.h"

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

	uint	Crc;		// 校验码
	uint	Crc2;		// 动态计算得到的校验码

	TokenMessage();

	bool Read(MemoryStream& ms);
	void Write(MemoryStream& ms);

	uint Size() const;

	// 设置数据
	void SetData(byte* buf, uint len);
	void SetError(byte error);
private:
};

// 处理消息，返回是否成功
typedef bool (*TokenMessageHandler)(TokenMessage& msg, void* param);

// 令牌控制器
class TokenController
{
private:
	ITransport* _port;

	static uint Dispatch(ITransport* transport, byte* buf, uint len, void* param);
	bool Dispatch(MemoryStream& ms, ITransport* port);

public:
	byte	Address;

	TokenController(ITransport* port);
	~TokenController();

	// 发送消息，传输口参数为空时向所有传输口发送消息
	bool Send(byte code, byte* buf = NULL, uint len = 0);
	// 发送消息，expire毫秒超时时间内，如果对方没有响应，会重复发送
	bool Send(TokenMessage& msg, int expire = -1);

	// 收到消息时触发
	TokenMessageHandler	Received;
	void*			Param;

// 处理器部分
private:
    struct HandlerLookup
    {
        uint			Code;	// 代码
        TokenMessageHandler	Handler;// 处理函数
		void*			Param;	// 参数
    };
	HandlerLookup* _Handlers[16];
	byte _HandlerCount;

};

#endif
