#ifndef _Net_H_
#define _Net_H_

#include "Sys.h"

// IP地址
class IPAddress : public Object
{
public:
	uint	Value;	// 地址

	IPAddress(int value)		{ Value = (uint)value; }
	IPAddress(uint value = 0)	{ Value = value; }
	IPAddress(const byte* ips);
	IPAddress(byte ip1, byte ip2, byte ip3, byte ip4);
	IPAddress(const ByteArray& arr);

    IPAddress& operator=(int v)			{ Value = (uint)v; return *this; }
    IPAddress& operator=(uint v)		{ Value = v; return *this; }
    IPAddress& operator=(const byte* v);

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    byte& operator[](int i);

	// 字节数组
    ByteArray ToArray() const;

	bool IsAny() const;
	bool IsBroadcast() const;
	uint GetSubNet(const IPAddress& mask) const;	// 获取子网

	// 输出对象的字符串表示方式
	virtual String& ToStr(String& str) const;

    friend bool operator==(const IPAddress& addr1, const IPAddress& addr2) { return addr1.Value == addr2.Value; }
    friend bool operator!=(const IPAddress& addr1, const IPAddress& addr2) { return addr1.Value != addr2.Value; }

	static const IPAddress& Any();
	static const IPAddress& Broadcast();
};

// IP结点
class IPEndPoint : public Object
{
public:
	IPAddress	Address;	// 地址
	ushort		Port;		// 端口

	IPEndPoint();
	IPEndPoint(const IPAddress& addr, ushort port);
	IPEndPoint(const ByteArray& arr);

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
	ulong	Value;	// 地址

	MacAddress(ulong v = 0);
	MacAddress(const byte* macs);
	MacAddress(const ByteArray& arr);

	// 是否广播地址，全0或全1
	bool IsBroadcast() const;

    MacAddress& operator=(ulong v);
    MacAddress& operator=(const byte* buf);

    // 重载索引运算符[]，让它可以像数组一样使用下标索引。
    byte& operator[](int i);

	// 字节数组
    ByteArray ToArray() const;

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
};

// Socket接口
class ISocket
{
public:
	ISocketHost*	Host;	// 主机

	IPEndPoint	Local;	// 本地地址。包含本地局域网IP地址，实际监听的端口，从1024开始累加
	IPEndPoint	Remote;	// 远程地址

	// 发送数据
	virtual bool Send(const ByteArray& bs) = 0;
	// 接收数据
	virtual uint Receive(ByteArray& bs) = 0;
};

#endif
