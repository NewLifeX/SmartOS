﻿#ifndef __Device_H__
#define __Device_H__

#include "Sys.h"
#include "Stream.h"

/******************************** Device ********************************/

// 设备信息
class Device : public Object
{
public:
	byte	Address;	// 节点地址
	ushort	Type;		// 类型
	ByteArray	HardID;	// 硬件编码
	ulong	LastTime;	// 活跃时间
	ushort	Version;	// 版本
	byte	DataSize;	// 数据大小
	byte	ConfigSize;	// 配置大小
	String	Name;		// 名称
	ByteArray	Pass;	// 通信密码

	ulong	RegTime;	// 注册时间
	ulong	LoginTime;	// 登录时间

	Device();

	void Write(Stream& ms) const;
	void Read(Stream& ms);

#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

bool operator==(const Device& d1, const Device& d2);
bool operator!=(const Device& d1, const Device& d2);

#endif