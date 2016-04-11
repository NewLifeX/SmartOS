#ifndef __TokenMessage_H__
#define __TokenMessage_H__

#include "Sys.h"
#include "Stream.h"
#include "Net\ITransport.h"

#include "Message\Controller.h"

// 令牌消息
class TokenMessage : public Message
{
public:
	/*byte	_Code:6;	// 功能码
	byte	_Error:1;	// 是否错误
	byte	_Reply:1;	// 是否响应指令
	byte	_Length;	// 数据长度*/

	byte	OneWay;		// 单向传输。无应答
	byte	Seq;		// 消息序号
	
	byte	_Data[256];	// 数据

	static const uint HeaderSize = 1 + 1 + 1;	// 消息头部大小
	static const uint MinSize = HeaderSize + 0;	// 最小消息大小

	// 使用指定功能码初始化令牌消息
	TokenMessage(byte code = 0);

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms) const;

	// 消息总长度，包括头部、负载数据和校验
	virtual uint Size() const;
	// 数据缓冲区大小
	virtual uint MaxDataSize() const;

	// 验证消息校验码是否有效
	virtual bool Valid() const;
	//// 设置错误信息字符串
	//void SetError(byte errorCode, const char* error, int errLength);

	// 创建当前消息对应的响应消息。设置序列号、标识位
	TokenMessage CreateReply() const;

	// 显示消息内容
	virtual void Show() const;
};

class TokenStat;

// 令牌控制器
class TokenController : public Controller
{
private:
	void*		Server;	// 服务器结点地址

protected:
	virtual bool Dispatch(Stream& ms, Message* pmsg, void* param);
	// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
	virtual bool Valid(const Message& msg);
	// 接收处理函数
	virtual bool OnReceive(Message& msg);

public:
	uint		Token;	// 令牌
	ByteArray	Key;	// 通信密码

	byte		NoLogCodes[8];	// 没有日志的指令

	TokenController();
	virtual ~TokenController();

	virtual void Open();
	virtual void Close();

	// 发送消息，传输口参数为空时向所有传输口发送消息
	virtual bool Send(Message& msg);
	// 发送消息，传输口参数为空时向所有传输口发送消息
	virtual bool Send(byte code, const Buffer& arr);

	// 响应消息
private:
	Message*	_Response;	// 等待响应的指令

	void ShowMessage(const char* action, const Message& msg);

public:
	// 发送消息并接受响应，msTimeout毫秒超时时间内，如果对方没有响应，会重复发送
	bool SendAndReceive(TokenMessage& msg, int retry = 0, int msTimeout = 20);

	// 统计
private:
	TokenStat*	Stat;

	uint		_taskID;

	void ShowStat();
	//static void StatTask(void* param);

	class QueueItem
	{
	public:
		byte	Code;
		UInt64	Time;	// 时间ms
	};

	QueueItem	_Queue[16];

	bool StartSendStat(byte code);
	bool EndSendStat(byte code, bool success);
};

// 令牌统计
class TokenStat : public Object
{
public:
	// 发送统计
	int	SendRequest;
	int	RecvReply;
	int	Time;

	String Percent() const;	// 成功率百分比，已乘以100
	int Speed() const;		// 平均速度，指令发出到收到响应的时间

	// 接收统计
	int	RecvRequest;
	int	SendReply;
	int	RecvReplyAsync;
	String PercentReply() const;

	// 数据操作统计
	int Read;
	int ReadReply;
	int Write;
	int WriteReply;

	TokenStat();
	~TokenStat();

	void Clear();

	virtual String& ToStr(String& str) const;

private:
	TokenStat*	_Last;
	TokenStat*	_Total;
};

#endif
