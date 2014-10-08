#ifndef _TinyIP_H_
#define _TinyIP_H_

// 模块开发使用说明见后

#include "List.h"
#include "Stream.h"
#include "Net\ITransport.h"
#include "Net\Net.h"

class TinyIP;

// 网络数据处理Socket基类
class Socket
{
public:
	TinyIP* Tip;
	ushort Type;

	Socket(TinyIP* tip);
	virtual ~Socket();

	// 处理数据包
	virtual bool Process(MemoryStream* ms) = 0;
};

// Socket列表
class SocketList : public List<Socket*>
{
public:
	Socket* FindByType(ushort type);
};

// 精简IP类
class TinyIP //: protected IEthernetAdapter
{
private:
	ITransport* _port;
	ulong _StartTime;

public:
	byte* Buffer; // 缓冲区

	// 任务函数
	static void Work(void* param);
	// 循环调度的任务，捕获数据包，返回长度
	uint Fetch(byte* buf = NULL, uint len = 0);
	// 处理数据包
	void Process(MemoryStream* ms);

public:
    IPAddress IP;	// 本地IP地址
    IPAddress Mask;	// 子网掩码
	MacAddress Mac;	// 本地Mac地址
	ushort Port;	// 本地端口
	bool EnableBroadcast;	// 使用广播
	bool EnableArp;	// 启用Arp

	MacAddress LocalMac;// 本地目标Mac地址
	IPAddress LocalIP;	// 本地目标IP地址
	//ushort LocalPort;	// 本地目标端口
	MacAddress RemoteMac;// 远程Mac地址
	IPAddress RemoteIP;	// 远程IP地址
	ushort RemotePort;	// 远程端口

	ushort BufferSize;	// 缓冲区大小
	IPAddress DHCPServer;
	IPAddress DNSServer;
	IPAddress Gateway;

	// Arp套接字
	Socket* Arp;
	// 套接字列表。套接字根据类型来识别
	//Socket* Sockets[0x20];
	//uint SocketCount;
	SocketList Sockets;

    TinyIP(ITransport* port);
    virtual ~TinyIP();

	bool Open();
	bool Init();
	void ShowInfo();
	static void ShowIP(IPAddress ip);
	static void ShowMac(const MacAddress& mac);
	static uint CheckSum(byte* buf, uint len, byte type);

	void SendEthernet(ETH_TYPE type, byte* buf, uint len);
	void SendIP(IP_TYPE type, byte* buf, uint len);
	bool IsMyIP(IPAddress ip);	// 是否发给我的IP地址
	bool IsBroadcast(IPAddress ip);	// 是否广播地址
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

#endif
