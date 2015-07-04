#ifndef __TinyServer_H__
#define __TinyServer_H__

#include "Sys.h"
#include "Message.h"
#include "Controller.h"
#include "TinyMessage.h"

/******************************** TinyServer ********************************/

class Device;

// 微网客户端
class TinyServer
{
private:
	TinyController* Control;

public:
	ushort	DeviceType;	// 设备类型。两个字节可做二级分类

	TinyServer(TinyController* control);

	// 发送消息
	bool Send(Message& msg);
	bool Reply(Message& msg);
	bool OnReceive(TinyMessage& msg);

	// 收到功能消息时触发
	MessageHandler	Received;
	void*			Param;

	List<Device*> Devices;
	Device* FindDevice(byte id);

// 常用系统级消息
public:
	// 设置默认系统消息
	void Start();

	// 广播发现系统
	bool OnDiscover(TinyMessage& msg);

	// 心跳保持与对方的活动状态
	bool OnPing(TinyMessage& msg);

	// 询问及设置系统时间
	bool OnSysTime(TinyMessage& msg);

	// 询问系统标识号
	bool OnSysID(TinyMessage& msg);

	// 设置系统模式
	bool OnSysMode(TinyMessage& msg);
};

/******************************** Device ********************************/

// 设备信息
class Device : public Object
{
public:
	byte	ID;			// 节点ID
	ushort	Type;		// 类型
	ByteArray	HardID;	// 物理ID
	ulong	LastTime;	// 活跃时间
	byte	Switchs;	// 开关数
	byte	Analogs;	// 通道数
	String	Name;		// 名称
	ByteArray	Pass;	// 通信密码

	ulong	RegTime;	// 注册时间
	ulong	LoginTime;	// 登录时间

	Device();

	void Write(Stream& ms) const;
	void Read(Stream& ms);

	virtual String& ToStr(String& str) const;
};

bool operator==(const Device& d1, const Device& d2);
bool operator!=(const Device& d1, const Device& d2);

#endif
