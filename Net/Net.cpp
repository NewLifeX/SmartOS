#include "Net.h"
#include "Config.h"

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

	auto ss	= ipstr.Split(".");
	for(int i=0; i<4 && ss; i++)
	{
		auto item	= ss.Next();
		if(item.Length() == 0 || item.Length() > 3) return ip;

		int v	= item.ToInt();
		if(v < 0 || v > 255) return ip;

		ip[i]	= (byte)v;
	}

	return ip;
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

String& IPEndPoint::ToStr(String& str) const
{
	Address.ToStr(str);

	//char ss[7];
	//int len = sprintf(ss, ":%d", Port);
	//str.Copy(ss, len, str.Length());
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

String& MacAddress::ToStr(String& str) const
{
	byte* macs = (byte*)&Value;

	for(int i=0; i<6; i++)
	{
		if(i > 0) str += '-';
		//str	+= macs[i];
		str.Concat(macs[i], -16);
	}

	return str;
}

/*void MacAddress::Show()
{
	byte* macs = (byte*)&Value;
	debug_printf("%02X-%02X-%02X-%02X-%02X-%02X", macs[0], macs[1], macs[2], macs[3], macs[4], macs[5]);
}*/

/******************************** ISocketHost ********************************/

struct NetConfig
{
	uint	IP;		// 本地IP地址
    uint	Mask;	// 子网掩码
	byte	Mac[6];	// 本地Mac地址

	uint	DHCPServer;
	uint	DNSServer;
	uint	Gateway;
};

bool ISocketHost::LoadConfig()
{
	if(!Config::Current) return false;

	NetConfig nc;
	Buffer bs(&nc, sizeof(nc));
	if(!Config::Current->Get("NET", bs)) return false;

	IP			= nc.IP;
	Mask		= nc.Mask;
	Mac			= nc.Mac;

	DHCPServer	= nc.DHCPServer;
	DNSServer	= nc.DNSServer;
	Gateway		= nc.Gateway;

	return true;
}

bool ISocketHost::SaveConfig()
{
	if(!Config::Current) return false;

	NetConfig nc;
	nc.IP	= IP.Value;
	nc.Mask	= Mask.Value;
	Mac.CopyTo(nc.Mac);

	nc.DHCPServer	= DHCPServer.Value;
	nc.DNSServer	= DNSServer.Value;
	nc.Gateway		= Gateway.Value;

	Buffer bs(&nc, sizeof(nc));
	return Config::Current->Set("NET", bs);
}

/*ISocket* ISocketHost::CreateSocket(ProtocolType type)
{
	return nullptr;
}*/
