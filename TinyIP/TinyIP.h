#ifndef _TinyIP_H_
#define _TinyIP_H_

// 模块开发使用说明见后

#include "Net\ITransport.h"
#include "Net\Ethernet.h"
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
	virtual bool Process(byte* buf, uint len) = 0;
};

// ARP协议
class ArpSocket : public Socket
{
private:
	// ARP表
	typedef struct
	{
		byte IP[4];
		byte Mac[6];
		uint Time;	// 生存时间，秒
	}ARP_ITEM;

	ARP_ITEM* _Arps;	// Arp表，动态分配

public:
	byte Count;	// Arp表行数，默认16行

	ArpSocket(TinyIP* tip);
	virtual ~ArpSocket();

	// 处理数据包
	virtual bool Process(byte* buf, uint len);

	// 请求Arp并返回其Mac。timeout超时3秒，如果没有超时时间，表示异步请求，不用等待结果
	const byte* Request(const byte ip[4], int timeout = 3);

	const byte* Resolve(const byte ip[4]);
	void Add(const byte ip[4], const byte mac[6]);
};

// ICMP协议
class IcmpSocket : public Socket
{
public:
	IcmpSocket(TinyIP* tip) : Socket(tip) { Type = IP_ICMP; }

	// 处理数据包
	virtual bool Process(byte* buf, uint len);

	// 收到Ping请求时触发，传递结构体和负载数据长度。返回值指示是否向对方发送数据包
	typedef bool (*PingHandler)(IcmpSocket* socket, ICMP_HEADER* icmp, byte* buf, uint len);
	PingHandler OnPing;

	// Ping目的地址，附带a~z重复的负载数据
	bool Ping(byte ip[4], uint payloadLength = 32);
};

// Tcp会话
class TcpSocket : public Socket
{
private:
	byte seqnum;

public:
	ushort Port;		// 本地端口
	byte RemoteIP[4];	// 远程地址
	ushort RemotePort;	// 远程端口

	TCP_HEADER* Tcp;

	TcpSocket(TinyIP* tip);

	// 处理数据包
	virtual bool Process(byte* buf, uint len);

	int Connect(byte ip[4], ushort port);	// 连接远程服务器，记录远程服务器IP和端口，后续发送数据和关闭连接需要
    void Send(byte* buf, uint len);			// 向Socket发送数据，可能是外部数据包
    void Close();	// 关闭Socket
	void Ack(uint len);

	void Send(TCP_HEADER* tcp, uint len, byte flags);

	void Head(uint ackNum, bool mss, bool cp_seq);

	// 收到Tcp数据时触发，传递结构体和负载数据长度。返回值指示是否向对方发送数据包
	typedef bool (*TcpHandler)(TcpSocket* socket, TCP_HEADER* tcp, byte* buf, uint len);
	TcpHandler OnAccepted;
	TcpHandler OnReceived;
	TcpHandler OnDisconnected;
};

// Udp会话
class UdpSocket : public Socket
{
public:
	ushort Port;	// 本地端口

	UdpSocket(TinyIP* tip) : Socket(tip) { Type = IP_UDP; }

	// 处理数据包
	virtual bool Process(byte* buf, uint len);

	// 收到Udp数据时触发，传递结构体和负载数据长度。返回值指示是否向对方发送数据包
	typedef bool (*UdpHandler)(UdpSocket* socket, UDP_HEADER* udp, byte* buf, uint len);
	UdpHandler OnReceived;

	// 发送UDP数据到目标地址
	void Send(byte* buf, uint len, byte ip[4], ushort port);
	//void ProcessUdp(byte* buf, uint len);
	void Send(byte* buf, uint len, bool checksum = true);
};

// DHCP协议
class Dhcp : UdpSocket
{
private:
	//uint dhcp_id;
	void Discover(DHCP_HEADER* dhcp);
	void Request(DHCP_HEADER* dhcp);
	void PareOption(byte* buf, uint len);

	void SendDhcp(DHCP_HEADER* dhcp, uint len);

public:
	//bool IPIsReady;
	//bool UseDHCP;

	Dhcp(TinyIP* tip) : UdpSocket(tip) { Type = IP_UDP; }

	bool Start();
};

// 精简IP类
class TinyIP //: protected IEthernetAdapter
{
public:
	ITransport* _port;
	//NetPacker* _net;

	byte* Buffer; // 缓冲区

	static void Work(void* param);	// 任务函数
	uint Fetch(byte* buf = NULL, uint len = 0);	// 循环调度的任务，捕获数据包，返回长度
	void Process(byte* buf, uint len);	// 处理数据包


public:
    byte IP[4];		// 本地IP地址
    byte Mask[4];	// 子网掩码
	byte Mac[6];	// 本地Mac地址
	ushort Port;	// 本地端口
	bool EnableBroadcast;	// 使用广播

	byte LocalMac[6];	// 本地目标Mac地址
	byte LocalIP[4];	// 本地目标IP地址
	//ushort LocalPort;	// 本地目标端口
	byte RemoteMac[6];	// 远程Mac地址
	byte RemoteIP[4];	// 远程IP地址
	ushort RemotePort;	// 远程端口

	ushort BufferSize;	// 缓冲区大小
	byte DHCPServer[4];
	byte DNSServer[4];
	byte Gateway[4];

	// Arp套接字
	ArpSocket* Arp;
	// 套接字列表。套接字根据类型来识别
	//Socket* Sockets[0x20];
	//uint SocketCount;
	List<Socket*> Sockets;

    TinyIP(ITransport* port, byte ip[4] = NULL, byte mac[6] = NULL);
    virtual ~TinyIP();

	bool Open();
	bool Init();
	static void ShowIP(const byte* ip);
	static void ShowMac(const byte* mac);
	static uint CheckSum(byte* buf, uint len, byte type);

	void SendEthernet(ETH_TYPE type, byte* buf, uint len);
	void SendIP(IP_TYPE type, byte* buf, uint len);
	bool IsMyIP(const byte ip[4]);	// 是否发给我的IP地址
	bool IsBroadcast(const byte ip[4]);	// 是否广播地址
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
