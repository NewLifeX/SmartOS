#ifndef __TokenMessage_H__
#define __TokenMessage_H__

#include "Sys.h"
#include "Stream.h"
#include "Net\ITransport.h"

#include "Message.h"
#include "Controller.h"

// 令牌消息
class TokenMessage : public Message
{
private:
	byte	_Data[256];	// 数据

public:
	uint	Token;		// 令牌

	byte	_Code:7;	// 功能码
	byte	Error:1;	// 是否异常

	byte	_Length;	// 数据长度

	uint	Checksum;		// 校验码
	uint	Crc;		// 动态计算得到的校验码

	static const uint HeaderSize = 4 + 1 + 1;	// 消息头部大小
	static const uint MinSize = HeaderSize + 0 + 4;	// 最小消息大小

	TokenMessage(byte code = 0);

	virtual bool Read(MemoryStream& ms);
	virtual void Write(MemoryStream& ms);

	virtual uint Size() const;

	// 验证消息校验码是否有效
	virtual bool Valid() const;
	// 计算当前消息的Crc
	virtual void ComputeCrc();

	void SetError(byte error);
};

// 令牌控制器
class TokenController : public Controller
{
private:
	void Init();

protected:
	// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
	virtual bool OnReceive(Message& msg, ITransport* port);

public:
	uint	Token;

	TokenController(ITransport* port);
	TokenController(ITransport* ports[], int count);
	virtual ~TokenController();

	// 创建消息
	virtual Message* Create() const;	
	// 发送消息，传输口参数为空时向所有传输口发送消息
	virtual int Send(Message& msg, ITransport* port = NULL);
	// 发送消息，传输口参数为空时向所有传输口发送消息
	virtual bool Send(byte code, byte* buf = NULL, uint len = 0);
	// 发送消息，expire毫秒超时时间内，如果对方没有响应，会重复发送
	//bool Send(TokenMessage& msg, int expire = -1);
};

#endif
