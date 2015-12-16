#ifndef __JoinMessage_H__
#define __JoinMessage_H__

#include "Message\MessageBase.h"

// 组网消息
/*
请求：1版本+2类型+4会话+N编码
响应：1网关+1通道+1速度+1节点地址+N密码+4会话+N网关编码
*/
class JoinMessage : public MessageBase
{
public:
	// 请求数据
	byte		Version;	// 组网指令的版本。与系统版本无关
	ushort		Kind;		// 类型
	uint		TranID;		// 会话
	ByteArray	HardID;		// 硬件编码。一般12~16字节
	String		Name;	// 名称

	// 响应数据
	byte		Server;		// 网关地址
	byte		WirKind;	//无线类型 
	ushort		PanID;		//无线网段
	byte		SendMode; 	//发送模式	
	byte		Channel;	// 通道
	byte		Speed;		// 传输速度
	byte		Address;	// 分配得到的设备地址
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
