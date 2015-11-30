#ifndef __SERVER__H__
#define __SERVER__H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"
#include "Message\DataStore.h"
#include "TokenClient.h"

#include "TinyNet\TinyServer.h"

#include "DeviceMessage.h"

// 网关服务器
class Gateway
{
public:
	TinyServer*		Server;		// 内网服务端
	TokenClient*	Client;	// 外网客户端
	IDataPort*		Led;		// 指示灯
	uint 			_taskStudy;

	Gateway();
	~Gateway();

	bool Running;
	void Start();
	void Stop();

	// 收到功能消息时触发
	MessageHandler Received;

	// 数据接收中心
	bool OnLocal(const TinyMessage& msg);
	bool OnRemote(const TokenMessage& msg);

	/******** 远程网业务逻辑 ********/
	bool AutoReport;	// 自动上报
	bool IsOldOrder; 	//是否旧指令

	void OldTinyToToken10(const TinyMessage& msg, TokenMessage& msg2);

	// 学习模式 0x20
	bool Study;
	void SetMode(bool student);
	void Clear();
	bool OnMode(const Message& msg);

	// 节点消息处理 0x21
	void DeviceRequest(DeviceAtions act, const Device* dv);
	bool DeviceProcess(const Message& msg);
	// 发送设备信息
	bool SendDevices(DeviceAtions act, const Device* dv);

	/******** 本地网业务逻辑 ********/
	// 设备发现
	bool OnDiscover(const TinyMessage& msg);

	static Gateway*	Current;
	static Gateway* CreateGateway(TokenClient* client, TinyServer* server);
	static void UpdateOnlneOfflne(void* param);
};

#endif
