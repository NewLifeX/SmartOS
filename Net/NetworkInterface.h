#ifndef _NetworkInterface_H_
#define _NetworkInterface_H_

#include "IPAddress.h"
#include "IPEndPoint.h"
#include "MacAddress.h"
#include "NetUri.h"
#include "Socket.h"

#include "Core\List.h"
#include "Core\Delegate.h"

class Socket;

// 网络类型
enum class NetworkType
{
	Wire	= 0,
	Station	= 1,
	AP		= 2,
	STA_AP	= 3,
};

// 网络状态
enum class NetworkStatus
{
	Unknown	= 0,	// 未知
	Up		= 1,	// 启用
	Down	= 2,	// 禁用
};

// 网络接口
class NetworkInterface
{
public:
	byte		Index;	// 索引
	bool		Active;	// 可用
	ushort		Speed;	// 速度Mbps。决定优先级
	cstring		Name;	// 名称

	IPAddress	IP;		// 本地IP地址
    IPAddress	Mask;	// 子网掩码
	MacAddress	Mac;	// 本地Mac地址
	NetworkType	Mode;	// 无线模式。0不是无线，1是STA，2是AP，3是混合
	NetworkStatus	Status;	// 网络状态

	IPAddress	Gateway;
	IPAddress	DNSServer;
	IPAddress	DNSServer2;

	Delegate<NetworkInterface&>	Changed;	// 网络改变
	Delegate<NetworkInterface&>	NetReady;	// 网络准备就绪。带this参数

	NetworkInterface();
	// 加上虚析构函数，因为应用层可能要释放该接口
	virtual ~NetworkInterface();

	// 打开与关闭
	virtual bool Open()	= 0;
	virtual void Close()= 0;
	
	// 应用配置
	virtual void Config() = 0;

	// 保存和加载动态获取的网络配置到存储设备
	void InitConfig();
	bool LoadConfig();
	bool SaveConfig();
	void ShowConfig();

	virtual Socket* CreateSocket(NetType type) = 0;

	// DNS解析。默认仅支持字符串IP地址解析
	virtual IPAddress QueryDNS(const String& domain);
	// 启用DNS
	virtual bool EnableDNS() { return false; }
	// 启用DHCP
	virtual bool EnableDHCP() { return false; }

	// 全局静态
	static List<NetworkInterface*> All;
};

/****************************** WiFiInterface ************************************/

// WiFi接口
class WiFiInterface : public NetworkInterface
{
public:
	String*		SSID;	// 无线SSID
	String*		Pass;	// 无线密码

	WiFiInterface();
	virtual ~WiFiInterface() { }

	bool IsStation() const;
	bool IsAP() const;

};

#endif
