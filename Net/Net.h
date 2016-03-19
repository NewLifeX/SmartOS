#ifndef _Net_H_
#define _Net_H_

#include "Sys.h"

// IP协议类型
enum ProtocolType
{
	Ip		= 0,
	Icmp	= 1,
	Igmp	= 2,
	Tcp		= 6,
	Udp		= 17,
};

// IP地址
class IPAddress : public Object
{
public:
	uint	Value;	// 地址

	IPAddress(int value)		{ Value = (uint)value; }
	IPAddress(uint value = 0)	{ Value = value; }
	IPAddress(const byte* ips);
	IPAddress(byte ip1, byte ip2, byte ip3, byte ip4);
	IPAddress(const Buffer& arr);

    IPAddress& operator=(int v)			{ Value = (uint)v; return *this; }
    IPAddress& operator=(uint v)		{ Value = v; return *this; }
    IPAddress& operator=(const byte* v);
    IPAddress& operator=(const Buffer& arr);

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    byte& operator[](int i);

	// 字节数组
    ByteArray ToArray() const;
	void CopyTo(byte* ips) const;

	bool IsAny() const;
	bool IsBroadcast() const;
	// 获取子网
	uint GetSubNet(const IPAddress& mask) const;

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;

    friend bool operator==(const IPAddress& addr1, const IPAddress& addr2) { return addr1.Value == addr2.Value; }
    friend bool operator!=(const IPAddress& addr1, const IPAddress& addr2) { return addr1.Value != addr2.Value; }

	static const IPAddress& Any();
	static const IPAddress& Broadcast();

	// 把字符串IP地址解析为IPAddress
	static IPAddress Parse(const String& ipstr);
};

// IP结点
class IPEndPoint : public Object
{
public:
	IPAddress	Address;	// 地址
	ushort		Port;		// 端口

	IPEndPoint();
	IPEndPoint(const IPAddress& addr, ushort port);
	IPEndPoint(const Buffer& arr);

    IPEndPoint& operator=(const Buffer& arr);

	// 字节数组
    ByteArray ToArray() const;
	void CopyTo(byte* ips) const;

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;

	static const IPEndPoint& Any();
};

bool operator==(const IPEndPoint& addr1, const IPEndPoint& addr2);
bool operator!=(const IPEndPoint& addr1, const IPEndPoint& addr2);

// Mac地址
class MacAddress : public Object
{
public:
	// 长整型转为Mac地址，取内存前6字节。因为是小字节序，需要v4在前，v2在后
	UInt64	Value;	// 地址

	MacAddress(UInt64 v = 0);
	MacAddress(const byte* macs);
	MacAddress(const Buffer& arr);

	// 是否广播地址，全0或全1
	bool IsBroadcast() const;

    MacAddress& operator=(UInt64 v);
    MacAddress& operator=(const byte* buf);
    MacAddress& operator=(const Buffer& arr);

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    byte& operator[](int i);

	// 字节数组
    ByteArray ToArray() const;
	void CopyTo(byte* macs) const;

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;

    friend bool operator==(const MacAddress& addr1, const MacAddress& addr2)
	{
		return addr1.Value == addr2.Value;
	}
    friend bool operator!=(const MacAddress& addr1, const MacAddress& addr2)
	{
		return addr1.Value != addr2.Value;
	}

	static const MacAddress& Empty();
	static const MacAddress& Full();
};

class ISocket;

// Socket主机
class ISocketHost
{
public:
	IPAddress	IP;		// 本地IP地址
    IPAddress	Mask;	// 子网掩码
	MacAddress	Mac;	// 本地Mac地址

	IPAddress	DHCPServer;
	IPAddress	DNSServer;
	IPAddress	Gateway;

	// 加上虚析构函数，因为应用层可能要释放该接口
	virtual ~ISocketHost() { }

	// 应用配置
	virtual void Config() = 0;

	// 保存和加载动态获取的网络配置到存储设备
	bool LoadConfig();
	bool SaveConfig();

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

	// 加上虚析构函数，因为应用层可能要释放该接口
	virtual ~ISocket() { }

	// 发送数据
	virtual bool Send(const Buffer& bs) = 0;
	virtual bool SendTo(const Buffer& bs, const IPEndPoint& remote) { return Send(bs); }
	// 接收数据
	virtual uint Receive(Buffer& bs) = 0;
};

#endif
