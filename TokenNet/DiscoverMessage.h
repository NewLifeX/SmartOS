#ifndef __DiscoverMessage_H__
#define __DiscoverMessage_H__

#include "MessageBase.h"

// 发现消息
//格式：2设备类型 + N系统ID
class DiscoverMessage : public MessageBase
{
public:
	ushort		Type;	// 类型
	ByteArray	HardID;	// 硬件ID

	// 初始化消息，各字段为0
	DiscoverMessage();

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms);
	
	virtual String& ToStr(String& str) const;
};

#endif
