#include "IPEndPoint.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

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

IPEndPoint::IPEndPoint(const Buffer& arr)
{
	/*byte* p = arr.GetBuffer();
	Address = p;
	Port	= *(ushort*)(p + 4);*/
	*this	= arr;
}

IPEndPoint& IPEndPoint::operator=(const Buffer& arr)
{
	Address	= arr;
	arr.CopyTo(4, &Port, 2);

	return *this;
}

// 字节数组
ByteArray IPEndPoint::ToArray() const
{
	//return ByteArray((byte*)&Value, 4);

	// 要复制数据，而不是直接使用指针，那样会导致外部修改内部数据
	ByteArray bs(&Address.Value, 4, true);
	bs.Copy(4, &Port, 2);

	return bs;
}

void IPEndPoint::CopyTo(byte* ips) const
{
	if(ips) ToArray().CopyTo(0, ips, 6);
}

String IPEndPoint::ToString() const
{
	auto str	= Address.ToString();

	str.Concat(':');
	str.Concat(Port);

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
