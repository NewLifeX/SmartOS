#ifndef __MINIGAT__H__
#define __MINIGAT__H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"
#include "Message\DataStore.h"
#include "TokenClient.h"

#include "TinyNet\TinyServer.h"

#include "DeviceMessage.h"

// 网关服务器
class MiniGateway
{
private:
	Device dev;
public:
	TinyServer*		Server;		// 内网服务端
	TokenClient*	Client;	// 外网客户端
	IDataPort*		Led;		// 指示灯

	MiniGateway();
	~MiniGateway();

	bool Running;
	void Start();
	void Stop();

	// 收到功能消息时触发
	MessageHandler Received;

	// 数据接收中心
	bool OnLocal(const TinyMessage& msg);
	bool OnRemote(const TokenMessage& msg);

	/******** 远程网业务逻辑 ********/
	// 学习模式 0x20
	void SetMode(uint sStudy);
	uint GetMode();
	void Clear();
	bool OnMode(const Message& msg);

	// 节点消息处理 0x21
	void DeviceRequest(DeviceAtions act, const Device* dv);
	bool DeviceProcess(const Message& msg);
	// 发送设备信息
	bool SendDevices(DeviceAtions act, const Device* dv);
	void SendDevicesIDs();

	/******** 本地网业务逻辑 ********/
	// 设备发现
	bool OnDiscover(const TinyMessage& msg);

	static MiniGateway*	Current;
	static MiniGateway* CreateMiniGateway(TokenClient* client, TinyServer* server);
	
public:
	uint	_task	= 0;	// 定时任务，10秒
	int		_Study	= 0;	// 自动退出学习时间，秒

	static void Loop(void* param);
};

#endif
