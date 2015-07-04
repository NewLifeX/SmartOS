#ifndef __SERVER__H__
#define __SERVER__H__

#include "Sys.h"
#include "Net\ITransport.h"
#include "TinyServer.h"
#include "TokenClient.h"

#include "TinyMessage.h"
#include "TokenMessage.h"

class Device;

// 网关服务器
class Gateway
{
private:

public:
	TinyServer* Server;		// 内网服务端
	TokenClient* Client;	// 外网客户端

	Gateway(TinyServer* server, TokenClient* client);
	~Gateway();

	bool Running;
	void Start();
	void Stop();

	// 收到功能消息时触发
	MessageHandler Received;

	// 数据接收中心
	bool OnLocal(TinyMessage& msg);
	bool OnRemote(TokenMessage& msg);

	/******** 远程网业务逻辑 ********/

	List<Device*> Devices;
	Device* FindDevice(byte id);

	// 设备列表 0x21
	bool OnGetDeviceList(Message& msg);
	// 设备信息 x025
	bool OnGetDeviceInfo(Message& msg);

	// 学习模式 0x20
	bool	Student;
	void SetMode(bool student);
	bool OnMode(Message& msg);

	// 节点注册入网 0x22
	void DeviceRegister(byte id);

	// 节点上线 0x23
	void DeviceOnline(byte id);

	// 节点离线 0x24
	void DeviceOffline(byte id);

	/******** 本地网业务逻辑 ********/
	// 设备发现
	bool OnDiscover(TinyMessage& msg);
};

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

	Device();
	
	void Write(Stream& ms) const;
	void Read(Stream& ms);

	virtual String& ToStr(String& str) const;
};

bool operator==(const Device& d1, const Device& d2);
bool operator!=(const Device& d1, const Device& d2);

#endif
