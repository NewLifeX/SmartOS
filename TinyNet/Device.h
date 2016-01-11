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
	bool	Logined;	// 是否在线
	byte	Address;	// 节点地址

	ushort	Kind;		// 类型
	byte	HardID[16];	// 硬件编码
	uint	LastTime;	// 活跃时间。秒
	uint	RegTime;	// 注册时间。秒
	uint	LoginTime;	// 登录时间。秒
	uint	Logins;		// 登录次数

	ushort	Version;	// 版本
	byte	DataSize;	// 数据大小
	byte	ConfigSize;	// 配置大小

	ushort	SleepTime;	// 睡眠时间。秒
	ushort	OfflineTime;// 离线阀值时间。秒
	ushort	PingTime;	// 心跳时间。秒

	byte	Mac[6];		// 无线物理地址
	char	Name[16];	// 名称
	//String	Name;		//变长名称
	byte	Pass[8];	// 通信密码

	byte	Store[32];	// 数据存储区

	// 以下字段不存Flash
	TinyConfig*	Cfg;

	uint	LastRead;	// 最后读取数据的时间。秒
	//uint	LastWrite;	// 最后写入数据的时间。毫秒

	Device();

	// 序列化到消息数据流
	void Write(Stream& ms) const;
	void Read(Stream& ms);
	void WriteMessage(Stream& ms) const;
	void ReadMessage(Stream& ms);

	// 保存到存储设备数据流
	void Save(Stream& ms) const;
	void Load(Stream& ms);

	bool CanSleep() const { return SleepTime > 0; }
	bool Valid() const;

	Array GetHardID();
	Array GetName();
	Array GetPass();
	Array GetStore();
	Array GetConfig();

	const Array GetHardID() const;
	const Array GetName() const;
	const Array GetPass() const;
	const Array GetStore() const;
	const Array GetConfig() const;

	void SetHardID(const Array& arr);
	void SetName(const Array& arr);
	void SetPass(const Array& arr);
	void SetStore(const Array& arr);
	void SetConfig(const Array& arr);

#if DEBUG
	virtual String& ToStr(String& str) const;
#endif
};

bool operator==(const Device& d1, const Device& d2);
bool operator!=(const Device& d1, const Device& d2);

#endif
