#ifndef __TinyServer_H__
#define __TinyServer_H__

#include "Sys.h"

#include "TinyMessage.h"
#include "TinyConfig.h"

#include "TokenNet\Device.h"
#include "TinyNet\DevicesManagement.h"

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

	//TinyServer 有足够的理由持有设备列表
	DevicesManagement DevMgmt;
	// 删除列表时候需要发送Disjoin消息  所以保留次函数
	void ClearDevices();
	// 云端处理设备时候回调
	void DevPrsCallback(DeviceAtions act, const Device* dv = nullptr);

	void SetChannel(byte channel);
	
	// 当前设备
	Device* Current;
	bool Study;		//学习模式

// 常用系统级消息
public:
	// 设置默认系统消息
	void Start();

	// 组网
	bool OnJoin(const TinyMessage& msg);
	
    bool Disjoin(TinyMessage& msg, ushort crc) const;
	bool Disjoin(byte id); 
	bool OnDisjoin(const TinyMessage& msg);

	// 心跳
	bool OnPing(const TinyMessage& msg);

	// 读取
	bool OnRead(const Message& msg, Message& rs, const Device& dv);
	bool OnReadReply(const Message& msg, Device& dv);

	// 写入
	bool OnWrite(const Message& msg, Message& rs, Device& dv);
	bool OnWriteReply(const Message& msg, Device& dv);
};

#endif
