#ifndef _IPEndPoint_H_
#define _IPEndPoint_H_

#include "IPAddress.h"

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

#endif
