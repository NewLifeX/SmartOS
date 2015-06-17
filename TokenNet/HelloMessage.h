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
	ushort		Version;// 版本
	String		Type;	// 类型
	String		Name;	// 名称
	DateTime	Time;	// 时间

	// 初始化消息，各字段为0
	HelloMessage(byte code = 0);
	HelloMessage(HelloMessage& msg);

	// 从数据流中读取消息
	virtual bool Read(Stream& ms) = 0;
	// 把消息写入数据流中
	virtual void Write(Stream& ms) = 0;

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
