#ifndef _Net_H_
#define _Net_H_

#include "IPAddress.h"
#include "IPEndPoint.h"
#include "MacAddress.h"

class ISocket;

// Socket主机
class ISocketHost
{
public:
	IPAddress	IP;		// 本地IP地址
    IPAddress	Mask;	// 子网掩码
	MacAddress	Mac;	// 本地Mac地址
	byte		Wireless;	// 无线模式。0不是无线，1是STA，2是AP，3是混合

	IPAddress	DHCPServer;
	IPAddress	DNSServer;
	IPAddress	DNSServer2;
	IPAddress	Gateway;

	// 加上虚析构函数，因为应用层可能要释放该接口
	virtual ~ISocketHost() { }

	// 应用配置
	virtual void Config() = 0;

	// 保存和加载动态获取的网络配置到存储设备
	void InitConfig();
	bool LoadConfig();
	bool SaveConfig();
	void ShowConfig();

	virtual ISocket* CreateSocket(ProtocolType type) = 0;
};

// Socket接口
class ISocket
{
public:
	ISocketHost*	Host;	// 主机
	ProtocolType	Protocol;	// 协议类型

	IPEndPoint	Local;	// 本地地址。包含本地局域网IP地址，实际监听的端口，从1024开始累加
	IPEndPoint	Remote;	// 远程地址
	String		Server;	// 远程地址，字符串格式，可能是IP字符串

	// 加上虚析构函数，因为应用层可能要释放该接口
	virtual ~ISocket() { }

	//// 应用配置，修改远程地址和端口
	//virtual bool Change(const String& remote, ushort port) { return false; };

	// 发送数据
	virtual bool Send(const Buffer& bs) = 0;
	virtual bool SendTo(const Buffer& bs, const IPEndPoint& remote) { return Send(bs); }
	// 接收数据
	virtual uint Receive(Buffer& bs) = 0;
};

#endif
