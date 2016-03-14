#ifndef _TinyIP_H_
#define _TinyIP_H_

// 模块开发使用说明见后

#include "Sys.h"
#include "Stream.h"
#include "Net\ITransport.h"
#include "Net\Net.h"
#include "Net\Ethernet.h"

class TinyIP;

// 网络数据处理Socket基类
class TinySocket : public Object
{
public:
	TinyIP*	Tip;	// TinyIP控制器
	IP_TYPE	Type;	// 类型
	bool	Enable;	// 启用

	TinySocket(TinyIP* tip, IP_TYPE type);
	virtual ~TinySocket();

	// 处理数据包
	virtual bool Process(IP_HEADER& ip, Stream& ms) = 0;
};

// Socket列表
class SocketList : public TArray<TinySocket*>
{
public:
	TinySocket* FindByType(ushort type);
	void Add(const TinySocket* socket);
	void Remove(const TinySocket* socket);
};

// 精简以太网协议。封装以太网帧以及IP协议，不包含其它协议实现，仅提供底层支持。
class TinyIP : public Object, public ISocketHost
{
private:
	ITransport*	_port;
	UInt64		_StartTime;
	ByteArray	_Arr; // 缓冲区

	// 循环调度的任务，捕获数据包，返回长度
	uint Fetch(Stream& ms);

	void Init();

public:

	// 任务函数
	static void Work(void* param);
	// 处理数据包
	void Process(Stream& ms);
	// 修正IP包负载数据的长度。物理层送来的长度可能有误，一般超长
	void FixPayloadLength(IP_HEADER& ip, Stream& ms);

public:
	// Arp套接字
	TinySocket*		Arp;
	// 套接字列表。套接字根据类型来识别
	SocketList	Sockets;

	TinyIP();
    TinyIP(ITransport* port);
    virtual ~TinyIP();
	void Init(ITransport* port);

	bool Open();
	virtual void Config();
	void ShowInfo();
	ushort CheckSum(IPAddress* remote, const byte* buf, uint len, byte type);

	bool SendEthernet(ETH_TYPE type, const MacAddress& remote, const byte* buf, uint len);
	bool SendIP(IP_TYPE type, const IPAddress& remote, const byte* buf, uint len);
	bool IsBroadcast(const IPAddress& ip);	// 是否广播地址

	virtual ISocket* CreateSocket(ProtocolType type);
};

/*
TinyIP作为精简以太网协议，每次只处理一个收发数据包，目标定位是超简单的使用场合！
ARP是必须有的，子网内计算机发数据包给它之前必须先通过ARP得到它的MAC地址
ARP还必须有一个表，记录部分IP和MAC的对照关系，子网内通讯时需要，子网外通讯也需要网关MAC
ICMP提供一个简单的Ping服务，可要可不要，用户习惯通过Ping来识别网络设备是否在线，因此默认使用该模块
TCP可作为服务端，处理任意端口请求并响应，这里不需要记录连接状态，因为客户端的每一次请求过来都有IP地址和MAC地址
TCP也可作为客户端，但只能使用固定几个连接，每次发送数据之前，还需要设置远程IP和端口
UDP可作为服务端，处理任意端口请求并响应
UDP也可作为客户端，向任意地址端口发送数据

因为只有一个缓冲区，所有收发数据包不能过大，并且只能有一包数据
*/

/*
网络架构：
1，Init初始化本地Mac地址，以及默认IP地址
2，Open打开硬件接口，实例化Arp，显示IP地址信息，添加以太网实时轮询Work
3，Work打包缓冲区Buffer为数据流，交付给Fetch获取数据，然后移交给Process处理数据
4，Fetch询问硬件接口是否有数据包，并展开以太网头部，检查是否本地MAC地址或者广播地址
5，Process检查Arp请求，然后检查IP请求、是否本机IP或广播数据包，记录来源IP以及MAC对，加快Arp速度
6，FixPayloadLength修正IP包负载数据的长度。物理层送来的长度可能有误，一般超长
7，Process最后交给各Socket.Process处理数据包
*/

#endif
