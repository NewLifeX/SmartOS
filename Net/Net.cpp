#include "Net.h"

/******************************** IPAddress ********************************/
/* IP地址 */

//const IPAddress IPAddress::Any(0, 0, 0, 0);
//const IPAddress IPAddress::Broadcast(255, 255, 255, 255);

IPAddress::IPAddress(const byte* ips)
{
	memcpy((byte*)&Value, ips, 4);
}

IPAddress::IPAddress(byte ip1, byte ip2, byte ip3, byte ip4)
{
	Value = (ip4 << 24) + (ip3 << 16) + (ip2 << 8) + ip1;
}

IPAddress::IPAddress(const ByteArray& arr)
{
	memcpy((byte*)&Value, arr.GetBuffer(), 4);
}

bool IPAddress::IsAny() const { return Value == 0; }

bool IPAddress::IsBroadcast() const { return Value == 0xFFFFFFFF; }

uint IPAddress::GetSubNet(const IPAddress& mask) const
{
	return Value & mask.Value;
}

const IPAddress& IPAddress::Any()
{
	static const IPAddress _Any(0, 0, 0, 0);
	return _Any;
}

const IPAddress& IPAddress::Broadcast()
{
	static const IPAddress _Broadcast(255, 255, 255, 255);
	return _Broadcast;
}

IPAddress& IPAddress::operator=(const byte* v)
{
	//Value = *(uint*)v;
	memcpy((byte*)&Value, v, 4);

	return *this;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
byte& IPAddress::operator[](int i)
{
	assert_param(i >= 0 && i < 4);

	return ((byte*)&Value)[i];
}

// 字节数组
ByteArray IPAddress::ToArray() const
{
	//return ByteArray((byte*)&Value, 4);

	// 要复制数据，而不是直接使用指针，那样会导致外部修改内部数据
	return ByteArray((byte*)&Value, 4, true);
}

String& IPAddress::ToStr(String& str) const
{
	byte* ips = (byte*)&Value;

	for(int i=0; i<4; i++)
	{
		if(i > 0) str.Append('.');
		str.Append((int)ips[i]);
	}

	return str;
}

/******************************** IPEndPoint ********************************/

//const IPEndPoint IPEndPoint::Any(IPAddress::Any, 0);

IPEndPoint::IPEndPoint()
{
	Address = 0;
	Port = 0;
}

IPEndPoint::IPEndPoint(const IPAddress& addr, ushort port)
{
	Address = addr;
	Port	= port;
}

IPEndPoint::IPEndPoint(const ByteArray& arr)
{
	byte* p = arr.GetBuffer();
	Address = p;
	Port	= *(ushort*)(p + 4);
}

String& IPEndPoint::ToStr(String& str) const
{
	Address.ToStr(str);

	//char ss[7];
	//int len = sprintf(ss, ":%d", Port);
	//str.Copy(ss, len, str.Length());
	str.Append(':').Append(Port);

	return str;
}

const IPEndPoint& IPEndPoint::Any()
{
	static const IPEndPoint _Any(IPAddress::Any(), 0);
	return _Any;
}

bool operator==(const IPEndPoint& addr1, const IPEndPoint& addr2)
{
	return addr1.Port == addr2.Port && addr1.Address == addr2.Address;
}
bool operator!=(const IPEndPoint& addr1, const IPEndPoint& addr2)
{
	return addr1.Port != addr2.Port || addr1.Address != addr2.Address;
}

/******************************** MacAddress ********************************/
/* MAC地址 */

#define MAC_MASK 0xFFFFFFFFFFFFull

//const MacAddress MacAddress::Empty(0x0ull);
//const MacAddress MacAddress::Full(MAC_MASK);

MacAddress::MacAddress(ulong v)
{
	Value = v & MAC_MASK;
}

MacAddress::MacAddress(const byte* macs)
{
	/*ByteArray bs(macs, 6);
	Value = bs.ToUInt64() & MAC_MASK;*/
	memcpy((byte*)&Value, macs, 6);
}

MacAddress::MacAddress(const ByteArray& arr)
{
	Value = arr.ToUInt64();
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

MacAddress& MacAddress::operator=(ulong v)
{
	Value = v & MAC_MASK;

	return *this;
}

MacAddress& MacAddress::operator=(const byte* buf)
{
	/*ByteArray bs(buf, 6);
	Value = bs.ToUInt64() & MAC_MASK;*/
	memcpy((byte*)&Value, buf, 6);

	return *this;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
byte& MacAddress::operator[](int i)
{
	assert_param(i >= 0 && i < 6);

	return ((byte*)&Value)[i];
}

// 字节数组
ByteArray MacAddress::ToArray() const
{
	//return ByteArray((byte*)&Value, 6);

	// 要复制数据，而不是直接使用指针，那样会导致外部修改内部数据
	return ByteArray((byte*)&Value, 6, true);
}

/*bool MacAddress::operator==(MacAddress& addr1, MacAddress& addr2)
{
	return addr1.v4 == addr2.v4 && addr1.v2 == addr2.v2;
}

bool MacAddress::operator!=(MacAddress& addr1, MacAddress& addr2)
{
	return addr1.v4 != addr2.v4 || addr1.v2 != addr2.v2;
}*/

String& MacAddress::ToStr(String& str) const
{
	byte* macs = (byte*)&Value;

	for(int i=0; i<6; i++)
	{
		if(i > 0) str.Append('-');
		str.Append(macs[i]);
	}

	return str;
}

/*void MacAddress::Show()
{
	byte* macs = (byte*)&Value;
	debug_printf("%02X-%02X-%02X-%02X-%02X-%02X", macs[0], macs[1], macs[2], macs[3], macs[4], macs[5]);
}*/
