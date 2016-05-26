#include "Net.h"
#include "Config.h"

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
		String sep(".");
		auto ss	= ipstr.Split(sep);
		for(int i=0; i<4 && ss; i++)
		{
			auto item	= ss.Next();
			if(item.Length() == 0 || item.Length() > 3) break;

			// 标准地址第一个不能是0，唯一的Any例外已经在前面处理
			int v	= item.ToInt();
			if(v < 0 || v > 255 || i == 0 && v == 0) break;

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
	mac.Show(true);
#endif

	return mac;
}

/******************************** ISocketHost ********************************/

struct NetConfig
{
	uint	IP;		// 本地IP地址
    uint	Mask;	// 子网掩码
	byte	Mac[6];	// 本地Mac地址
	byte	Wireless;	// 无线模式。0不是无线，1是STA，2是AP，3是混合
	byte	Reserved;

	uint	DHCPServer;
	uint	DNSServer;
	uint	DNSServer2;
	uint	Gateway;
};

void ISocketHost::InitConfig()
{
	IPAddress defip(192, 168, 1, 1);

	// 随机IP，取ID最后一个字节
	IP = defip;
	byte first = Sys.ID[0];
	if(first <= 1 || first >= 254) first = 2;
	IP[3] = first;

	Mask = IPAddress(255, 255, 255, 0);
	DHCPServer = Gateway = defip;

	// 修改为双DNS方案，避免单点故障。默认使用阿里和百度公共DNS。
	DNSServer	= IPAddress(223, 5, 5, 5);
	DNSServer2	= IPAddress(180, 76, 76, 76);

	auto& mac = Mac;
	// 随机Mac，前2个字节取自ASCII，最后4个字节取自后三个ID
	//mac[0] = 'W'; mac[1] = 'S';
	// 第一个字节最低位为1表示组播地址，所以第一个字节必须是偶数
	mac[0] = 'N'; mac[1] = 'X';
	for(int i=0; i< 4; i++)
		mac[2 + i] = Sys.ID[3 - i];

	Wireless	= 0;
}

bool ISocketHost::LoadConfig()
{
	if(!Config::Current) return false;

	NetConfig nc;
	Buffer bs(&nc, sizeof(nc));
	if(!Config::Current->Get("NET", bs)) return false;

	IP			= nc.IP;
	Mask		= nc.Mask;
	Mac			= nc.Mac;
	Wireless	= nc.Wireless;

	DHCPServer	= nc.DHCPServer;
	DNSServer	= nc.DNSServer;
	DNSServer2	= nc.DNSServer2;
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
	nc.Wireless	= Wireless;

	nc.DHCPServer	= DHCPServer.Value;
	nc.DNSServer	= DNSServer.Value;
	nc.DNSServer2	= DNSServer2.Value;
	nc.Gateway		= Gateway.Value;

	Buffer bs(&nc, sizeof(nc));
	return Config::Current->Set("NET", bs);
}

void ISocketHost::ShowConfig()
{
#if NET_DEBUG
	net_printf("    MAC:\t");
	Mac.Show();
	net_printf("\r\n    IP:\t");
	IP.Show();
	net_printf("\r\n    Mask:\t");
	Mask.Show();
	net_printf("\r\n    Gate:\t");
	Gateway.Show();
	net_printf("\r\n    DHCP:\t");
	DHCPServer.Show();
	net_printf("\r\n    DNS:\t");
	DNSServer.Show();
	if(!DNSServer2.IsAny())
	{
		net_printf("\r\n    DNS2:\t");
		DNSServer2.Show();
	}
	net_printf("\r\n    模式:\t");
	if(!Wireless)
		net_printf("有线");
	else
	{
		switch(Wireless)
		{
			case 1:
				net_printf("无线Station");
				break;
			case 2:
				net_printf("无线AP热点");
				break;
			case 3:
				net_printf("无线Station+AP热点");
				break;
		}
		net_printf("\r\n");
	}
	net_printf("\r\n");
#endif
}
