#ifndef __Memory_H__
#define __Memory_H__

#include "Sys.h"

// 登录消息
class LoginMessage : public MessageBase
{
public:
	ByteArray	HardID;	// 硬件ID
	ByteArray	Key;	// 登录密码
	ByteArray	Salt;	// 加盐
	IPEndPoint	Local;	// 内网地址

	bool		Reply;	// 是否响应

	// 初始化消息，各字段为0
	LoginMessage();

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms);

	// 显示消息内容
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

#endif
