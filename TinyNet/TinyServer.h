#ifndef __TinyServer_H__
#define __TinyServer_H__

#include "Sys.h"

#include "TinyMessage.h"

#include "TinyNet\Device.h"

/******************************** TinyServer ********************************/

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
	Device* FindDevice(ByteArray& hardid);
	bool	DeleteDevice(byte id);

	// 当前设备
	Device* Current;
	
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

#endif
