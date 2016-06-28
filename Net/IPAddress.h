#ifndef _IPAddress_H_
#define _IPAddress_H_

#include "Core\ByteArray.h"
#include "Core\SString.h"

// IP协议类型
enum class ProtocolType
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

#endif
