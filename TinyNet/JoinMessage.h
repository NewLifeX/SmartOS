#ifndef __JoinMessage_H__
#define __JoinMessage_H__

#include "Message\MessageBase.h"

/*
请求：HardVersion（硬件版本1byte）+SoftVersion（软件版本1byte）+Kind（类型2byte）+HardID（cpuID      16byte）+TranID（会话ID 4byte）
响应：GID（网关ID 1byte）+Channel（通道号 1byte）+ ID（节点ID 1byte）+Password（密码 变长）+ TranID（会话ID 4byte）
*/
// 组网消息
class JoinMessage : public MessageBase
{
public:
	// 请求数据
	byte		HardVer;
	byte		SoftVer;
	ushort		Kind;		// 类型
	ByteArray	HardID;		// 硬件ID
	ushort		TranID;		// 会话ID

	// 响应数据
	byte		Server;		// 网关ID
	byte		Channel;	// 通道
	byte		Speed;		// 传输速度
	byte		Address;	// 分配得到的设备ID
	ByteArray	Password;	// 通信密码。一般8字节
	
	// 初始化消息，各字段为0
	JoinMessage();

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms);
	
#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

#endif
