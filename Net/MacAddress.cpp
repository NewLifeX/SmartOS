#include "Core\_Core.h"

#include "Core\ByteArray.h"
#include "Core\SString.h"

#include "MacAddress.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

/******************************** MacAddress ********************************/
/* MAC地址 */

#define MAC_MASK 0xFFFFFFFFFFFFull

//const MacAddress MacAddress::Empty(0x0ull);
//const MacAddress MacAddress::Full(MAC_MASK);

MacAddress::MacAddress(UInt64 v)
{
	Value = v & MAC_MASK;
}

MacAddress::MacAddress(const byte* macs)
{
	Buffer(&Value, 6)	= macs;
}

MacAddress::MacAddress(const Buffer& arr)
{
	ByteArray bs(arr.GetBuffer(), arr.Length());
	Value = bs.ToUInt64();
	Value &= MAC_MASK;
}

// 是否广播地址，全0或全1
bool MacAddress::IsBroadcast() const { return Value == 0 || Value == MAC_MASK; }

const MacAddress& MacAddress::Empty()
{
	static const MacAddress _Empty(0x0ull);
	return _Empty;
}

const MacAddress& MacAddress::Full()
{
	static const MacAddress _Full(MAC_MASK);
	return _Full;
}

MacAddress& MacAddress::operator=(UInt64 v)
{
	Value = v & MAC_MASK;

	return *this;
}

MacAddress& MacAddress::operator=(const byte* buf)
{
	Buffer((byte*)buf, 6).CopyTo(0, &Value, 6);

	return *this;
}

MacAddress& MacAddress::operator=(const Buffer& arr)
{
	arr.CopyTo(0, &Value, 6);

	return *this;
}

MacAddress& MacAddress::operator=(const MacAddress& mac)
{
	Value	= mac.Value;

	return *this;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
byte& MacAddress::operator[](int i)
{
	return ((byte*)&Value)[i];
}

// 字节数组
ByteArray MacAddress::ToArray() const
{
	//return ByteArray((byte*)&Value, 6);

	// 要复制数据，而不是直接使用指针，那样会导致外部修改内部数据
	return ByteArray(&Value, 6, true);
}

void MacAddress::CopyTo(byte* macs) const
{
	if(macs) Buffer((byte*)&Value, 6).CopyTo(0, macs, 6);
}

String MacAddress::ToString() const
{
	String str;
	byte* macs = (byte*)&Value;

	for(int i=0; i<6; i++)
	{
		if(i > 0) str += '-';
		//str	+= macs[i];
		str.Concat(macs[i], -16);
	}

	return str;
}

// 把字符串Mac地址解析为MacAddress
MacAddress MacAddress::Parse(const String& macstr)
{
	auto mac	= MacAddress::Empty();
	if(!macstr) return mac;

	// 最大长度判断 00-00-00-00-00-00
	if(macstr.Length() > 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2 + 1 + 2) return mac;

	auto str	= macstr.Replace(':', '-');
	if(str.Contains("-"))
	{
		// 特殊处理 00-00-00-00-00-00 和 FF-FF-FF-FF-FF-FF
		if(macstr == "00-00-00-00-00-00") return mac;
		if(macstr == "FF-FF-FF-FF-FF-FF") return MacAddress::Full();

		auto bs		= str.ToHex();
		if(bs.Length() == 6)
		{
			mac	= bs;
			return mac;
		}
	}

#if NET_DEBUG
	// 只显示失败
	net_printf("MacAddress::Parse %s => ", macstr.GetBuffer());
	mac.ToString().Show(true);
#endif

	return mac;
}
