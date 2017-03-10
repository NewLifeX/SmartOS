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
	None	= 0,	// 未知
	Up		= 1,	// 启用
	Down	= 2,	// 禁用
};

// 网络接口
class NetworkInterface
{
public:
	cstring		Name;	// 名称
	ushort		Speed;	// 速度Mbps。决定优先级
	bool		Opened;	// 接口已上电打开，检测到网卡存在
	bool		Linked;	// 接口网络就绪，可以通信
	NetworkType	Mode;	// 无线模式。0不是无线，1是STA，2是AP，3是混合
	NetworkStatus	Status;	// 网络状态

	IPAddress	IP;		// 本地IP地址
    IPAddress	Mask;	// 子网掩码
	MacAddress	Mac;	// 本地Mac地址

	IPAddress	Gateway;
	IPAddress	DNSServer;
	IPAddress	DNSServer2;

	Delegate<NetworkInterface&>	Changed;	// 网络改变

	NetworkInterface();
	// 加上虚析构函数，因为应用层可能要释放该接口
	virtual ~NetworkInterface();

	// 打开与关闭
	bool Open();
	void Close();
	bool Active() const;

	// 应用配置
	virtual bool Config() = 0;

	// 保存和加载网络配置
	void InitConfig();
	bool LoadConfig();
	bool SaveConfig() const;
	void ShowConfig() const;

	virtual Socket* CreateSocket(NetType type) = 0;
	Socket* CreateClient(const NetUri& uri);
	Socket* CreateRemote(const NetUri& uri);

	// DNS解析。默认仅支持字符串IP地址解析
	virtual IPAddress QueryDNS(const String& domain);
	// 启用DNS
	virtual bool EnableDNS() { return false; }
	// 启用DHCP
	virtual bool EnableDHCP() { return false; }

protected:
	uint	_taskLink;
	uint	_retry;

	// 打开与关闭
	virtual bool OnOpen()	= 0;
	virtual void OnClose()	= 0;

	// 循环检测连接
	virtual void OnLoop();
	virtual bool OnLink(uint retry) { return true; }
	virtual bool CheckLink() { return Linked; }

	// 保存和加载网络配置
	virtual void OnInitConfig() {}
	virtual void OnLoadConfig(const Buffer& buf) {}
	virtual void OnSaveConfig(Buffer& buf) const {}
	virtual void OnShowConfig() const {}

public:
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
	virtual ~WiFiInterface();

	bool IsStation() const;
	bool IsAP() const;

	// WiFi无需连接路由器就可以建立本地监听
	Socket* CreateClient(const NetUri& uri);

protected:
	// 保存和加载网络配置
	virtual void OnInitConfig();
	virtual void OnLoadConfig(const Buffer& buf);
	virtual void OnSaveConfig(Buffer& buf) const;
	virtual void OnShowConfig() const;
};

#endif
