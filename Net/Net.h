#ifndef _Net_H_
#define _Net_H_

#include "Sys.h"

// TCP/IP协议头部结构体


// 字节序
#ifndef LITTLE_ENDIAN
	#define LITTLE_ENDIAN   1
#endif

#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)

// IP地址
typedef uint IPAddress;
// Mac地址。结构体和类都可以
//typedef struct _MacAddress MacAddress;
//struct _MacAddress
class MacAddress
{
public:
	// 长整型转为Mac地址，取内存前6字节。因为是小字节序，需要v4在前，v2在后
	// 比如Mac地址12-34-56-78-90-12，v4是12-34-56-78，v2是90-12，ulong是0x0000129078563412
	uint v4;
	ushort v2;

	MacAddress(ulong v = 0)
	{
		v4 = v;
		v2 = v >> 32;
	}
	
	// 是否广播地址，全0或全1
	bool IsBroadcast() { return !v4 && !v2 || v4 == 0xFFFFFFFF && v2 == 0xFFFF; }

    MacAddress& operator=(ulong v)
	{
		v4 = v;
		v2 = v >> 32;

		// 下面这个写法很好，一条汇编指令即可完成，但是会覆盖当前结构体后两个字节
		//*(ulong*)this = v;

		// 下面的写法需要5条汇编指令，先放入内存，再分两次读写
		/*uint* p = (uint*)&v;
		v4 = *p++;
		v2 = *(ushort*)p;*/

		return *this;
	}
    ulong Value()
	{
		ulong v = v4;
		v |= ((ulong)v2) << 32;
		return v;

		// 下面这个写法简单，但是会带上后面两个字节，需要做或运算，不划算
		//return *(ulong*)this | 0x0000FFFFFFFF;
	}
    friend bool operator==(MacAddress& addr1, MacAddress& addr2)
	{
		return addr1.v4 == addr2.v4 && addr1.v2 == addr2.v2;
	}
    friend bool operator!=(MacAddress& addr1, MacAddress& addr2)
	{
		return addr1.v4 != addr2.v4 || addr1.v2 != addr2.v2;
	}
};
//}MacAddress;

#define IP_FULL 0xFFFFFFFF
#define MAC_FULL 0xFFFFFFFFFFFFFFFFull

// 以太网协议类型
typedef enum
{
	ETH_ARP = 0x0608,
	ETH_IP = 0x0008,
	ETH_IPv6 = 0xDD86,
}ETH_TYPE;

#define IS_ETH_TYPE(type) (type == ETH_ARP || type == ETH_IP || type == ETH_IPv6)

//Mac头部，总长度14字节
typedef struct _ETH_HEADER
{
	MacAddress DestMac;	// 目标mac地址
	MacAddress SrcMac;	// 源mac地址
	ETH_TYPE Type;		// 以太网类型

	uint Size() { return sizeof(this[0]); }
	uint Offset() { return Size(); }
	byte* Next() { return (byte*)this + Size(); }
}ETH_HEADER;

// IP协议类型
typedef enum
{
	IP_ICMP = 1,
	IP_IGMP = 2,
	IP_TCP = 6,
	IP_UDP = 17,
}IP_TYPE;

#define IS_IP_TYPE(type) (type == IP_ICMP || type == IP_IGMP || type == IP_TCP || type == IP_UDP)

// IP头部，总长度20=0x14字节，偏移14=0x0E。后面可能有可选数据，Length决定头部总长度（4的倍数）
typedef struct _IP_HEADER
{
	#if LITTLE_ENDIAN
	byte Length:4;  // 首部长度
	byte Version:4; // 版本
	#else
	byte Version:4; // 版本
	byte Length:4;  // 首部长度。每个单位4个字节
	#endif
	byte TypeOfService; // 服务类型
	ushort TotalLength;	// 总长度
	ushort Identifier;	// 标志
	byte Flags;			// 标识是否对数据包进行分段
	byte FragmentOffset;// 记录分段的偏移值。接收者会根据这个值进行数据包的重新组和
	byte TTL;			// 生存时间
	IP_TYPE Protocol;	// 协议
	ushort Checksum;	// 检验和
	IPAddress SrcIP;	// 源IP地址
	IPAddress DestIP;	// 目的IP地址

	void Init(IP_TYPE type, bool recursion = false)
	{
		Version = 4;
		Length = sizeof(this[0]) >> 2;
		TypeOfService = 0;
		Flags = 0;
		FragmentOffset = 0;
		TTL = 64;
		Protocol = type;

		if(recursion) Prev()->Type = ETH_IP;
	}

	uint Size() { return (Length <= 5) ? sizeof(this[0]) : (Length << 2); }
	uint Offset() { return Prev()->Offset() + Size(); }
	ETH_HEADER* Prev() { return (ETH_HEADER*)((byte*)this - sizeof(ETH_HEADER)); }
	//byte* Next() { return (byte*)this + sizeof(&this[0]); }
	byte* Next() { return (byte*)this + Size(); }
}IP_HEADER;

typedef enum
{
	TCP_FLAGS_FIN = 1,		// 结束连接请求标志位。为1表示结束连接请求数据包
	TCP_FLAGS_SYN = 2,		// 连接请求标志位。为1表示发起连接的请求数据包
	TCP_FLAGS_RST = 4,
	TCP_FLAGS_PUSH = 8,		// 标志位，为1表示此数据包应立即进行传递
	TCP_FLAGS_ACK = 0x10,	// 应答标志位，为1表示确认，数据包为应答数据包
	TCP_FLAGS_URG = 0x20,
}TCP_FLAGS;

//TCP头部，总长度20=0x14字节，偏移34=0x22。后面可能有可选数据，Length决定头部总长度（4的倍数）
typedef struct _TCP_HEADER
{
	ushort SrcPort;		// 源端口号
	ushort DestPort;    // 目的端口号
	uint Seq;			// 序列号
	uint Ack;	        // 确认号
	#if LITTLE_ENDIAN
	byte reserved_1:4;	// 保留6位中的4位首部长度
	byte Length:4;		// tcp头部长度
	byte Flags:6;		// 6位标志
	byte reserved_2:2;	// 保留6位中的2位
	#else
	byte Length:4;		// tcp头部长度
	byte reserved_1:4;	// 保留6位中的4位首部长度
	byte reserved_2:2;	// 保留6位中的2位
	byte Flags:6;		// 6位标志
	#endif
	ushort WindowSize;	// 16位窗口大小
	ushort Checksum;	// 16位TCP检验和
	ushort urgt_p;		// 16为紧急指针

	void Init(bool recursion = false)
	{
		Length = sizeof(this[0]);
		reserved_1 = 0;
		reserved_2 = 0;
		WindowSize = __REV16(8192);
		urgt_p = 0;

		if(recursion) Prev()->Init(IP_TCP, recursion);
	}

	uint Size() { return (Length <= 5) ? sizeof(this[0]) : (Length << 2); }
	uint Offset() { return Prev()->Offset() + Size(); }
	IP_HEADER* Prev() { return (IP_HEADER*)((byte*)this - sizeof(IP_HEADER)); }
	byte* Next() { return (byte*)this + Size(); }
}TCP_HEADER;

//UDP头部，总长度8字节，偏移34=0x22
typedef struct _UDP_HEADER
{
	ushort SrcPort;		// 远端口号
	ushort DestPort;	// 目的端口号
	ushort Length;      // udp头部长度
	ushort Checksum;	// 16位udp检验和

	void Init(bool recursion = false)
	{
		Length = sizeof(this[0]);

		if(recursion) Prev()->Init(IP_UDP, recursion);
	}

	uint Size() { return sizeof(this[0]); }
	uint Offset() { return Prev()->Offset() + Size(); }
	IP_HEADER* Prev() { return (IP_HEADER*)((byte*)this - sizeof(IP_HEADER)); }
	byte* Next() { return (byte*)this + Size(); }
}UDP_HEADER;

//ICMP头部，总长度8字节，偏移34=0x22
typedef struct _ICMP_HEADER
{
	byte Type;			// 类型
	byte Code;			// 代码
	ushort Checksum;    // 16位检验和
	ushort Identifier;	// 标识，仅用于Ping
	ushort Sequence;	// 序列号，仅用于Ping

	void Init(bool recursion = false)
	{
		Type = 8;
		Code = 0;

		if(recursion) Prev()->Init(IP_ICMP, recursion);
	}

	uint Size() { return sizeof(this[0]); }
	uint Offset() { return Prev()->Offset() + Size(); }
	IP_HEADER* Prev() { return (IP_HEADER*)((byte*)this - sizeof(IP_HEADER)); }
	byte* Next() { return (byte*)this + Size(); }
}ICMP_HEADER;

// ARP头部，总长度28=0x1C字节，偏移14=0x0E，可能加18字节填充
typedef struct _ARP_HEADER
{
	ushort HardType;		// 硬件类型
	ushort ProtocolType;	// 协议类型
	byte HardLength;		// 硬件地址长度
	byte ProtocolLength;	// 协议地址长度
	ushort Option;			// 选项
	MacAddress SrcMac;
	IPAddress SrcIP;		// 源IP地址
	MacAddress DestMac;
	IPAddress DestIP;		// 目的IP地址
	//byte Padding[18];	// 填充凑够60字节

	void Init(bool recursion = false)
	{
		HardType = 0x0100;
		ProtocolType = 0x08;
		HardLength = 6;
		ProtocolLength = 4;

		if(recursion) Prev()->Type = ETH_ARP;
	}

	uint Size() { return sizeof(this[0]); }
	uint Offset() { return Prev()->Offset() + Size(); }
	ETH_HEADER* Prev() { return (ETH_HEADER*)((byte*)this - sizeof(ETH_HEADER)); }
	byte* Next() { return (byte*)this + Size(); }
}ARP_HEADER;

// DHCP头部，总长度240=0xF0字节，偏移42=0x2A，后面可选数据偏移282=0x11A
typedef struct _DHCP_HEADER
{
	byte MsgType;		// 若是client送给server的封包，设为1，反向为2
	byte HardType;		// 以太网1
	byte HardLength;	// 以太网6
	byte Hops;			// 若数据包需经过router传送，每站加1，若在同一网内，为0
	uint TransID;		// 事务ID，是个随机数，用于客户和服务器之间匹配请求和相应消息
	ushort Seconds;		// 由用户指定的时间，指开始地址获取和更新进行后的时间
	ushort Flags;		// 从0-15bits，最左一bit为1时表示server将以广播方式传送封包给 client，其余尚未使用
	IPAddress ClientIP;	// 客户机IP
	IPAddress YourIP;	// 你的IP
	IPAddress ServerIP;	// 服务器IP
	IPAddress RelayIP;	// 中继代理IP
	byte ClientMac[16];	// 客户端硬件地址
	byte ServerName[64];// 服务器名
	byte BootFile[128];	// 启动文件名
	uint Magic;		// 幻数0x63825363，小端0x63538263

	void Init(uint dhcpid, bool recursion = false)
	{
		// 为了安全，清空一次
		memset(this, 0, sizeof(this[0]));

		MsgType = 1;
		HardType = 1;
		HardLength = 6;
		Hops = 0;
		TransID = __REV(dhcpid);
		Flags = 0x80;	// 从0-15bits，最左一bit为1时表示server将以广播方式传送封包给 client，其余尚未使用
		SetMagic();

		if(recursion) Prev()->Init(recursion);
	}

	uint Size() { return sizeof(this[0]); }
	uint Offset() { return Prev()->Offset() + Size(); }
	UDP_HEADER* Prev() { return (UDP_HEADER*)((byte*)this - sizeof(UDP_HEADER)); }
	byte* Next() { return (byte*)this + Size(); }

	void SetMagic() { Magic = 0x63538263; }
	bool Valid() { return Magic == 0x63538263; }
}DHCP_HEADER;

// DHCP后面可选数据格式为“代码+长度+数据”

typedef enum
{
	DHCP_OPT_Mask = 1,
	DHCP_OPT_Router = 3,
	DHCP_OPT_TimeServer = 4,
	DHCP_OPT_NameServer = 5,
	DHCP_OPT_DNSServer = 6,
	DHCP_OPT_LOGServer = 7,
	DHCP_OPT_HostName = 12,
	DHCP_OPT_MTU = 26,				// 0x1A
	DHCP_OPT_StaticRout = 33,		// 0x21
	DHCP_OPT_ARPCacheTimeout = 35,	// 0x23
	DHCP_OPT_NTPServer = 42,		// 0x2A
	DHCP_OPT_RequestedIP = 50,		// 0x32
	DHCP_OPT_IPLeaseTime = 51,		// 0x33
	DHCP_OPT_MessageType = 53,		// 0x35
	DHCP_OPT_DHCPServer = 54,		// 0x36
	DHCP_OPT_ParameterList = 55,	// 0x37
	DHCP_OPT_Message = 56,			// 0x38
	DHCP_OPT_MaxMessageSize = 57,	// 0x39
	DHCP_OPT_Vendor = 60,			// 0x3C
	DHCP_OPT_ClientIdentifier = 61,	// 0x3D
	DHCP_OPT_End = 255,
}DHCP_OPTION;

typedef enum
{
	DHCP_TYPE_Discover = 1,
	DHCP_TYPE_Offer = 2,
	DHCP_TYPE_Request = 3,
	DHCP_TYPE_Decline = 4,
	DHCP_TYPE_Ack = 5,
	DHCP_TYPE_Nak = 6,
	DHCP_TYPE_Release = 7,
	DHCP_TYPE_Inform = 8,
}DHCP_MSGTYPE;

typedef struct _DHCP_OPT
{
	DHCP_OPTION Option;// 代码
	byte Length;	// 长度
	byte Data;		// 数据

	struct _DHCP_OPT* Next() { return (struct _DHCP_OPT*)((byte*)this + 2 + Length); }

	struct _DHCP_OPT* SetType(DHCP_MSGTYPE type)
	{
		Option = DHCP_OPT_MessageType;
		Length = 1;
		Data = type;

		return this;
	}

	struct _DHCP_OPT* SetData(DHCP_OPTION option, byte* buf, uint len)
	{
		Option = option;
		Length = len;
		memcpy(&Data, buf, Length);

		return this;
	}

	struct _DHCP_OPT* SetData(DHCP_OPTION option, uint value)
	{
		Option = option;
		Length = 4;
		//memcpy(&Data, (byte*)&value, Length);
		*(uint*)&Data = value;

		return this;
	}

	struct _DHCP_OPT* SetClientId(byte* mac, uint len = 6)
	{
		Option = DHCP_OPT_ClientIdentifier;
		Length = 1 + len;
		Data = 1;	// 类型ETHERNET=1
		memcpy(&Data + 1, mac, len);

		return this;
	}

	struct _DHCP_OPT* End()
	{
		Option = DHCP_OPT_End;

		return this;
	}
}DHCP_OPT;

#pragma pack(pop)	// 恢复对齐状态

#endif
