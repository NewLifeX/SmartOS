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

public:
	byte	_Code:7;	// 功能码
	byte	_Reply:1;	// 是否响应指令
	byte	_Length;	// 数据长度

	byte	_Data[256];	// 数据

	static const uint HeaderSize = 1 + 1;	// 消息头部大小
	static const uint MinSize = HeaderSize + 0;	// 最小消息大小

	// 使用指定功能码初始化令牌消息
	TokenMessage(byte code = 0);

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms);

	// 消息总长度，包括头部、负载数据和校验
	virtual uint Size() const;

	// 验证消息校验码是否有效
	virtual bool Valid() const;
	virtual void ComputeCrc() { }
	// 设置错误信息字符串
	void SetError(byte errorCode, string error, int errLength);

	// 显示消息内容
	virtual void Show() const;
};

class TokenStat;

// 令牌控制器
class TokenController : public Controller
{
private:

protected:
	virtual bool Dispatch(Stream& ms, Message* pmsg, ITransport* port);
	// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
	virtual bool Valid(Message& msg, ITransport* port);
	// 接收处理函数
	virtual bool OnReceive(Message& msg, ITransport* port);

public:
	uint		Token;	// 令牌
	ByteArray	Key;	// 通信密码

	TokenController();

	virtual void Open();
	virtual void Close();

	// 发送消息，传输口参数为空时向所有传输口发送消息
	virtual int Send(Message& msg, ITransport* port = NULL);
	// 发送消息，传输口参数为空时向所有传输口发送消息
	virtual bool Send(byte code, byte* buf = NULL, uint len = 0);

	// 响应消息
private:
	Message*	_Response;	// 等待响应的指令

public:
	// 发送消息并接受响应，msTimeout毫秒超时时间内，如果对方没有响应，会重复发送
	bool SendAndReceive(TokenMessage& msg, int retry = 0, int msTimeout = 20);
	
	// 统计
private:
	TokenStat* Stat;
	
	class QueueItem
	{
	public:
		byte	Code;
		ulong	Time;
	};
	
	QueueItem	_Queue[16];
	
	void StartSendStat(byte code);
	void EndSendStat(byte code, bool success);
};

// 令牌会话
class TokenSession
{
public:
	uint	Token;	// 当前会话的令牌
};

// 令牌统计
class TokenStat
{
public:
	int		Send;
	int		Success;
	int		Time;
	int		Receive;

	int Percent();	// 成功率百分比，已乘以10000
	int Speed();	// 平均速度，指令发出到收到响应的时间

	~TokenStat();
	
	void Start();

private:
	TokenStat*	_Last;
	TokenStat*	_Total;

	int		_taskID;

	void ClearStat();
	static void StatTask(void* param);

public:

};

#endif
