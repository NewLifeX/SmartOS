#ifndef __DeviceMessage_H__
#define __DeviceMessage_H__

#include "Message\MessageBase.h"
#include "Net\Net.h"

enum class DeviceAtions
{
	// <summary>信息列表。请求ID列表，响应设备信息列表</summary>
	List,

	// <summary>更新。请求设备信息列表，响应ID列表</summary>
	Update,

	// <summary>注册。请求设备信息列表，响应ID列表</summary>
	Register,

	// <summary>上线。请求响应ID列表</summary>
	Online,

	// <summary>下线。请求响应ID列表</summary>
	Offline,

	// <summary>删除。请求响应ID列表</summary>
	Delete,
	// <summary>ID列表</summary>
	ListIDs,
};

// 设备消息
class DeviceMessage : public MessageBase
{
public:
	ByteArray	HardID;	// 硬件ID
	ByteArray	Key;	// 登录密码
	ByteArray	Salt;	// 加盐
	IPEndPoint	Local;	// 内网地址

	bool		Reply;	// 是否响应

	// 初始化消息，各字段为0
	DeviceMessage();

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms) const;

	// 显示消息内容
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

#endif
