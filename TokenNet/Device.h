#ifndef __Device_H__
#define __Device_H__

#include "Sys.h"

#include "Flash.h"
#include "TinyNet\TinyConfig.h"

/******************************** Device ********************************/

union DevFlag
{
	ushort Data;
	struct
	{
		ushort OnlineAlws : 1;	// 持久在线
		ushort Reserved : 15;	// 保留
	}BitFlag;
};


// 设备信息
class Device : public Object
{
public:
	bool	Logined;	// 是否在线
	uint	LastTime;	// 活跃时间。秒
	uint	LoginTime;	// 登录时间。秒
	uint	Logins;		// 登录次数

	byte	Address;	// 节点地址

	ushort	Kind;		// 类型
	byte	_HardID[16];// 硬件编码
	uint	RegTime;	// 注册时间。秒

	ushort	Version;	// 版本
	byte	DataSize;	// 数据大小
	byte	ConfigSize;	// 配置大小

	ushort	SleepTime;	// 睡眠时间。秒
	ushort	OfflineTime;// 离线阀值时间。秒
	ushort	PingTime;	// 心跳时间。秒

	byte	_Mac[6];	// 无线物理地址
	char	_Name[16];	// 名称
	//String	Name;	// 变长名称
	byte	_Pass[8];	// 通信密码
	DevFlag  Flag;		// 其他状态

	// 在Tiny网络下  作为设备的数据区的网关备份 加快访问速度
	// 在Token网络下  直接作为虚拟设备的数据区
	byte	_Store[32];	// 数据存储区 （主数据区）

	// 以下字段不存Flash
	TinyConfig*	Cfg;

	uint	LastRead;	// 最后读取数据的时间。秒
	//uint	LastWrite;	// 最后写入数据的时间。毫秒

	Buffer	HardID;
	Buffer	Mac;
	String	Name;
	Buffer	Pass;
	Buffer	Store;
	//Buffer	Config;
	
	Device();

	// 序列化到消息数据流
	void Write(Stream& ms) const;
	void Read(Stream& ms);
	/*
	void WriteMessage(Stream& ms) const;
	void ReadMessage(Stream& ms);
	*/
	bool CanSleep() const { return SleepTime > 0; }
	bool Valid() const;

#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

bool operator==(const Device& d1, const Device& d2);
bool operator!=(const Device& d1, const Device& d2);


#endif
