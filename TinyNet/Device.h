#ifndef __Device_H__
#define __Device_H__

#include "Sys.h"
#include "Stream.h"

#include "TinyConfig.h"

/******************************** Device ********************************/

// 设备信息
class Device : public Object
{
public:
	byte		Address;	// 节点地址
	bool		Logined;	// 是否在线

	ushort		Kind;		// 类型
	ByteArray	HardID;		// 硬件编码
	uint		LastTime;	// 活跃时间。秒
	uint		Logins;		// 登录次数
	ushort		Version;	// 版本
	byte		DataSize;	// 数据大小
	byte		ConfigSize;	// 配置大小
	String		Name;		// 名称
	ByteArray	Pass;		// 通信密码

	ushort		PingTime;	// 心跳时间。秒
	ushort		OfflineTime;// 离线阀值时间。秒
	ushort		SleepTime;	// 睡眠时间。秒

	uint		RegTime;	// 注册时间。秒
	uint		LoginTime;	// 登录时间。秒

	ByteArray	Store;		// 数据存储区
	TinyConfig*	Cfg	= NULL;

	Device();

	// 序列化到消息数据流
	void Write(Stream& ms) const;
	void Read(Stream& ms);

	// 保存到存储设备数据流
	void Save(Stream& ms) const;
	void Load(Stream& ms);

	bool CanSleep() const { return SleepTime > 0; }

#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

bool operator==(const Device& d1, const Device& d2);
bool operator!=(const Device& d1, const Device& d2);

#endif
