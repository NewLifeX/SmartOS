#ifndef __TinyServer_H__
#define __TinyServer_H__

#include "Sys.h"

#include "TinyMessage.h"
#include "TinyConfig.h"

#include "TinyNet\Device.h"

/******************************** TinyServer ********************************/

// 微网服务端
class TinyServer
{
public:
	TinyController* Control;
	TinyConfig*	Cfg;

	ushort	DeviceType;	// 设备类型。两个字节可做二级分类

	TinyServer(TinyController* control);

	// 发送消息
	bool Send(Message& msg) const;
	//bool Reply(Message& msg) const;
	// 收到本地无线网消息
	bool OnReceive(TinyMessage& msg);
	// 分发外网过来的消息。返回值表示是否有响应
	bool Dispatch(TinyMessage& msg);

	// 收到功能消息时触发
	MessageHandler	Received;
	void*			Param;

	TArray<Device*> Devices;
	Device* FindDevice(byte id) const;
	Device* FindDevice(const Array& hardid) const;
	bool	DeleteDevice(byte id);

	int LoadDevices();
	void SaveDevices() const;
	void ClearDevices();

	void SetChannel(byte channel);
	
	void WriteCfg(TinyMessage& msg)const;
	bool LoadConfig();
	void SaveConfig() const;
	void ClearConfig();	
	// 当前设备
	Device* Current;
	bool Study;		//学习模式

// 常用系统级消息
public:
	// 设置默认系统消息
	void Start();

	// 组网
	bool OnJoin(const TinyMessage& msg);

	bool ResetPassword(byte id) const;
	
    bool Disjoin(TinyMessage& msg,uint crc) const;
	bool OnDisjoin(const TinyMessage& msg);

	// 心跳
	bool OnPing(const TinyMessage& msg);

	// 读取
	bool OnRead(const Message& msg, Message& rs, Device& dv);
	bool OnReadReply(const TinyMessage& msg, Device& dv);

	// 写入
	bool OnWrite(const Message& msg, Message& rs, Device& dv);
};

#endif
