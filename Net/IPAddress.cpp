#include "Sys.h"

#include "IPAddress.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

/******************************** IPAddress ********************************/
/* IP地址 */

//const IPAddress IPAddress::Any(0, 0, 0, 0);
//const IPAddress IPAddress::Broadcast(255, 255, 255, 255);

IPAddress::IPAddress(const byte* ips)
{
	Buffer((byte*)ips, 4).CopyTo(0, &Value, 4);
}

IPAddress::IPAddress(byte ip1, byte ip2, byte ip3, byte ip4)
{
	Value = (ip4 << 24) + (ip3 << 16) + (ip2 << 8) + ip1;
}

IPAddress::IPAddress(const Buffer& arr)
{
	arr.CopyTo(0, &Value, 4);
}

bool IPAddress::IsAny() const { return Value == 0; }

bool IPAddress::IsBroadcast() const { return Value == 0xFFFFFFFF; }

uint IPAddress::GetSubNet(const IPAddress& mask) const
{
	return Value & mask.Value;
}

// 把字符串IP地址解析为IPAddress
IPAddress IPAddress::Parse(const String& ipstr)
{
	auto ip	= IPAddress::Any();
	if(!ipstr) return ip;

	// 最大长度判断 255.255.255.255
	if(ipstr.Length() > 3 + 1 + 3 + 1 + 3 + 1 + 3) return ip;

	if(ipstr.Contains("."))
	{
		// 特殊处理 0.0.0.0 和 255.255.255.255
		if(ipstr == "0.0.0.0") return ip;
		if(ipstr == "255.255.255.255") return IPAddress::Broadcast();

		// 这里不能在Split参数直接使用字符指针，隐式构造的字符串对象在这个函数之后将会被销毁
		auto sp	= ipstr.Split(".");
		for(int i=0; i<4 && sp; i++)
		{
			auto item	= sp.Next();
			if(item.Length() == 0 || item.Length() > 3) break;

			// 标准地址第一个不能是0，唯一的Any例外已经在前面处理
			int v	= item.ToInt();
			if(v < 0 || v > 255 || (i == 0 && v == 0)) break;

			ip[i]	= (byte)v;

			// 只有完全4个都正确，才直接返回
			if(i == 4-1) return ip;
		}
	}
#if NET_DEBUG
	// 只显示失败
	net_printf("IPAddress::Parse %s => %08X \r\n", ipstr.GetBuffer(), ip.Value);
#endif

	return IPAddress::Any();
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
	Buffer((byte*)v, 4).CopyTo(0, &Value, 4);

	return *this;
}

IPAddress& IPAddress::operator=(const Buffer& arr)
{
	arr.CopyTo(0, &Value, 4);

	return *this;
}

// 重载索引运算符[]，让它可以像数组一样使用下标索引。
byte& IPAddress::operator[](int i)
{
	return ((byte*)&Value)[i];
}

// 字节数组
ByteArray IPAddress::ToArray() const
{
	//return ByteArray((byte*)&Value, 4);

	// 要复制数据，而不是直接使用指针，那样会导致外部修改内部数据
	return ByteArray(&Value, 4, true);
}

void IPAddress::CopyTo(byte* ips) const
{
	if(ips) Buffer((byte*)&Value, 4).CopyTo(0, ips, 4);
}

String& IPAddress::ToStr(String& str) const
{
	byte* ips = (byte*)&Value;

	for(int i=0; i<4; i++)
	{
		if(i > 0) str	+= '.';
		str += (int)ips[i];
	}

	return str;
}
