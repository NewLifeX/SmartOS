#ifndef __TokenMessage_H__
#define __TokenMessage_H__

#include "Message\Message.h"

enum ErrorCodeType :byte
{
	NoError,		// 没错
	DecryptError,	// 解密失败
	SeqError,		// Seq无效
};

// 令牌消息
class TokenMessage : public Message
{
public:
	//byte	OneWay;		// 单向传输。无应答
	byte	Seq;		// 消息序号
	ErrorCodeType	ErrorCode;	// 错误类型   仅本地使用，不与云端交互
	
	byte	_Data[512];	// 数据

	static const int HeaderSize = 1 + 1 + 1;	// 消息头部大小
	static const int MinSize = HeaderSize + 0;	// 最小消息大小

	// 使用指定功能码初始化令牌消息
	TokenMessage(byte code = 0);

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms) const;

	// 消息总长度，包括头部、负载数据和校验
	virtual int Size() const;
	// 数据缓冲区大小
	virtual int MaxDataSize() const;

	// 验证消息校验码是否有效
	virtual bool Valid() const;

	// 创建当前消息对应的响应消息。设置序列号、标识位
	TokenMessage CreateReply() const;

	// 显示消息内容
	virtual void Show() const;
};

#endif
