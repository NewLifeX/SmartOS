#ifndef _MacAddress_H_
#define _MacAddress_H_

#include "Core\Buffer.h"

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

	// 把字符串Mac地址解析为MacAddress
	static MacAddress Parse(const String& macstr);
};

#endif
