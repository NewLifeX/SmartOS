#ifndef __HelloMessage_H__
#define __HelloMessage_H__

#include "Sys.h"
#include "Stream.h"

// 握手消息
// 请求：2版本 + S类型 + S名称 + 8本地时间 + 本地IP端口 + S支持加密算法列表
// 响应：2版本 + S类型 + S名称 + 8对方时间 + 对方IP端口 + S加密算法 + N密钥
class HelloMessage
{
public:
	byte	Code;	// 消息代码
	byte	Length;	// 数据长度
	byte*	Data;	// 数据。指向子类内部声明的缓冲区
	byte	Reply;	// 是否响应指令

	// 初始化消息，各字段为0
	HelloMessage(byte code = 0);
	HelloMessage(HelloMessage& msg);

	// 消息所占据的指令数据大小。包括头部、负载数据、校验和附加数据
	virtual uint Size() const = 0;

	// 从数据流中读取消息
	virtual bool Read(MemoryStream& ms) = 0;
	// 把消息写入数据流中
	virtual void Write(MemoryStream& ms) = 0;

	// 验证消息校验码是否有效
	virtual bool Valid() const = 0;
	// 计算当前消息的Crc
	virtual void ComputeCrc() = 0;
	// 设置数据
	void SetData(byte* buf, uint len);

	// 显示消息内容
	virtual void Show() const = 0;
};

#endif
