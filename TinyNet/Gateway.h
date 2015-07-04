#ifndef __SERVER__H__
#define __SERVER__H__

#include "Sys.h"
#include "Net\ITransport.h"
#include "TinyServer.h"
#include "TokenClient.h"

#include "TinyMessage.h"
#include "TokenMessage.h"

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

#endif
