#ifndef _Net_H_
#define _Net_H_

#include "Sys.h"

// TCP/IP协议头部结构体


// 字节序
#ifndef LITTLE_ENDIAN
	#define LITTLE_ENDIAN   (1)   //BYTE ORDER
#else
	#error Redefine LITTLE_ORDER
#endif

// 强制结构体紧凑分配空间
#pragma pack(1)

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
	unsigned char DestMac[6]; //目标mac地址
	unsigned char SrcMac[6]; //源mac地址
	ETH_TYPE Type; //以太网类型

	byte* Next() { return (byte*)this + sizeof(this[0]); }
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
	unsigned char Length:4;  //首部长度
	unsigned char Version:4; //版本
	#else
	unsigned char Version:4; //版本
	unsigned char Length:4;  //首部长度。每个单位4个字节
	#endif
	unsigned char TypeOfService;       //服务类型
	unsigned short TotalLength;	//总长度
	unsigned short Identifier;	//标志
	unsigned char Flags;		// 标识是否对数据包进行分段
	unsigned char FragmentOffset;	// 记录分段的偏移值。接收者会根据这个值进行数据包的重新组和
	unsigned char TTL;			//生存时间
	IP_TYPE Protocol;		//协议
	unsigned short Checksum;	//检验和
	unsigned char SrcIP[4];		//源IP地址
	unsigned char DestIP[4];	//目的IP地址

	void Init(IP_TYPE type, bool recursion = false)
	{
		Version = 4;
		Length = sizeof(this[0]) >> 2;
		Protocol = type;
		
		if(recursion) Prev()->Type = ETH_IP;
	}

	ETH_HEADER* Prev() { return (ETH_HEADER*)((byte*)this - sizeof(this[0])); }
	//byte* Next() { return (byte*)this + sizeof(&this[0]); }
	byte* Next() { return (byte*)this + ((Length <= 5) ? sizeof(this[0]) : (Length << 2)); }
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
	unsigned short SrcPort;    //源端口号
	unsigned short DestPort;    //目的端口号
	unsigned int Seq;        //序列号
	unsigned int Ack;        //确认号
	#if LITTLE_ENDIAN
	unsigned char reserved_1:4; //保留6位中的4位首部长度
	unsigned char Length:4;        //tcp头部长度
	unsigned char Flags:6;       //6位标志
	unsigned char reseverd_2:2; //保留6位中的2位
	#else
	unsigned char Length:4;        //tcp头部长度
	unsigned char reserved_1:4; //保留6位中的4位首部长度
	unsigned char reseverd_2:2; //保留6位中的2位
	unsigned char Flags:6;       //6位标志
	#endif
	unsigned short WindowSize;    //16位窗口大小
	unsigned short Checksum;     //16位TCP检验和
	unsigned short urgt_p;      //16为紧急指针

	void Init(bool recursion = false)
	{
		
		if(recursion) Prev()->Init(IP_TCP, recursion);
	}

	IP_HEADER* Prev() { return (IP_HEADER*)((byte*)this - sizeof(this[0])); }
	byte* Next() { return (byte*)this + ((Length <= 5) ? sizeof(this[0]) : (Length << 2)); }
}TCP_HEADER;

//UDP头部，总长度8字节，偏移34=0x22
typedef struct _UDP_HEADER
{
	unsigned short SrcPort; //远端口号
	unsigned short DestPort; //目的端口号
	unsigned short Length;      //udp头部长度
	unsigned short Checksum;  //16位udp检验和

	void Init(bool recursion = false)
	{
		Length = sizeof(this[0]);
		
		if(recursion) Prev()->Init(IP_UDP, recursion);
	}

	IP_HEADER* Prev() { return (IP_HEADER*)((byte*)this - sizeof(this[0])); }
	byte* Next() { return (byte*)this + sizeof(this[0]); }
}UDP_HEADER;

//ICMP头部，总长度8字节，偏移34=0x22
typedef struct _ICMP_HEADER
{
	unsigned char Type;			//类型
	unsigned char Code;			//代码
	unsigned short Checksum;    //16位检验和
	unsigned short Identifier;	//标识，仅用于Ping
	unsigned short Sequence;	//序列号，仅用于Ping

	void Init(bool recursion = false)
	{
		
		if(recursion) Prev()->Init(IP_ICMP, recursion);
	}

	IP_HEADER* Prev() { return (IP_HEADER*)((byte*)this - sizeof(this[0])); }
	byte* Next() { return (byte*)this + sizeof(this[0]); }
}ICMP_HEADER;

// ARP头部，总长度28=0x1C字节，偏移14=0x0E，可能加18字节填充
typedef struct _ARP_HEADER
{
	unsigned short HardType;		// 硬件类型
	unsigned short ProtocolType;	// 协议类型
	unsigned char HardLength;		// 硬件地址长度
	unsigned char ProtocolLength;	// 协议地址长度
	unsigned short Option;			// 选项
	unsigned char SrcMac[6];
	unsigned char SrcIP[4];		//源IP地址
	unsigned char DestMac[6];
	unsigned char DestIP[4];	//目的IP地址
	//unsigned char Padding[18];	// 填充凑够60字节

	void Init(bool recursion = false)
	{
		HardType = 0x0100;
		ProtocolType = 0x08;
		HardLength = 6;
		ProtocolLength = 4;

		if(recursion) Prev()->Type = ETH_ARP;
	}

	ETH_HEADER* Prev() { return (ETH_HEADER*)((byte*)this - sizeof(this[0])); }
	byte* Next() { return (byte*)this + sizeof(this[0]); }
}ARP_HEADER;

// DHCP头部，总长度240=0xF0字节，偏移42=0x2A，后面可选数据偏移282=0x11A
typedef struct _DHCP_HEADER
{
	unsigned char MsgType;		// 若是client送给server的封包，设为1，反向为2
	unsigned char HardType;		// 以太网1
	unsigned char HardLength;	// 以太网6
	unsigned char Hops;			// 若数据包需经过router传送，每站加1，若在同一网内，为0
	unsigned int TransID;		// 事务ID，是个随机数，用于客户和服务器之间匹配请求和相应消息
	unsigned short Seconds;		// 由用户指定的时间，指开始地址获取和更新进行后的时间
	unsigned short Flags;		// 从0-15bits，最左一bit为1时表示server将以广播方式传送封包给 client，其余尚未使用
	unsigned char ClientIP[4];	// 客户机IP
	unsigned char YourIP[4];	// 你的IP
	unsigned char ServerIP[4];	// 服务器IP
	unsigned char RelayIP[4];	// 中继代理IP
	unsigned char ClientMac[16];	// 客户端硬件地址
	unsigned char ServerName[64];	// 服务器名
	unsigned char BootFile[128];	// 启动文件名
	unsigned int Magic;		// 幻数0x63825363，小端0x63538263

	UDP_HEADER* Prev() { return (UDP_HEADER*)((byte*)this - sizeof(this[0])); }
	byte* Next() { return (byte*)this + sizeof(this[0]); }

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
	DHCP_OPT_MTU = 26,
	DHCP_OPT_StaticRout = 33,
	DHCP_OPT_ARPCacheTimeout = 35,
	DHCP_OPT_NTPServer = 42,
	DHCP_OPT_RequestedIP = 50,
	DHCP_OPT_IPLeaseTime = 51,
	DHCP_OPT_MessageType = 53,
	DHCP_OPT_DHCPServer = 54,
	DHCP_OPT_ParameterList = 55,
	DHCP_OPT_Message = 56,
	DHCP_OPT_MaxMessageSize = 57,
	DHCP_OPT_Vendor = 60,
	DHCP_OPT_ClientIdentifier = 61,
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

// 网络封包机
class NetPacker
{
private:
	byte* Buffer;

public:
	NetPacker(byte* buf)
	{
		Buffer = buf;
		Eth = (ETH_HEADER*)buf;
	}

	ETH_HEADER* Eth;
	uint TotalLength;	// 数据总长度

	ARP_HEADER* ARP;
	IP_HEADER* IP;
	ICMP_HEADER* ICMP;
	TCP_HEADER* TCP;
	UDP_HEADER* UDP;
	byte* Payload;	// 负载数据
	uint PayloadLength; // 负载数据长度

	// 解包。把参数拆分出来，主要涉及各指针长度
	bool Unpack(uint len)
	{
		if(len < sizeof(ETH_HEADER)) return false;

		TotalLength = len;

		// 计算负载。不同协议负载不一样，后面可能还会再次进行计算
		Payload = (byte*)Eth + sizeof(ETH_HEADER);
		PayloadLength = len - sizeof(ETH_HEADER);
		switch(Eth->Type)
		{
			case ETH_ARP:
			{
				ARP = (ARP_HEADER*)Payload;
				IP = NULL;
				Payload += sizeof(ARP_HEADER);
				PayloadLength -= sizeof(ARP_HEADER);
				break;
			}
			case ETH_IP:
			{
				IP = (IP_HEADER*)Payload;
				ARP = NULL;

				// IP包后面可能有附加数据。长度是4的倍数
				uint iplen = IP->Length << 2;
				if(iplen < sizeof(IP_HEADER)) iplen = sizeof(IP_HEADER);
				Payload += iplen;
				PayloadLength -= iplen;
				// 前面的len不准确，必须以这个为准
				PayloadLength = __REV16(IP->TotalLength) - iplen;

				ICMP = NULL;
				TCP = NULL;
				UDP = NULL;

				switch(IP->Protocol)
				{
					case IP_ICMP:
					{
						ICMP = (ICMP_HEADER*)Payload;
						Payload += sizeof(ICMP_HEADER);
						PayloadLength -= sizeof(ICMP_HEADER);
						break;
					}
					case IP_TCP:
					{
						TCP = (TCP_HEADER*)Payload;
						iplen = TCP->Length << 2;
						if(iplen < sizeof(TCP_HEADER)) iplen = sizeof(TCP_HEADER);
						Payload += iplen;
						PayloadLength -= iplen;
						break;
					}
					case IP_UDP:
					{
						UDP = (UDP_HEADER*)Payload;
						Payload += sizeof(UDP_HEADER);
						PayloadLength -= sizeof(UDP_HEADER);
						break;
					}
				}

				break;
			}
		}

		return Payload <= (byte*)Eth + len;
	}

	// 封包。把参数组装回去
	void Pack();

	DHCP_HEADER* GetDHCP()
	{
		if(PayloadLength < sizeof(DHCP_HEADER)) return NULL;

		return (DHCP_HEADER*)Payload;
	}

	ARP_HEADER* SetARP()
	{
		IP = NULL;
		ARP = (ARP_HEADER*)Eth->Next();
		ARP->HardType = 0x0100;
		ARP->ProtocolType = 0x08;
		ARP->HardLength = 6;
		ARP->ProtocolLength = 4;

		return ARP;
	}

	// 设置使用IP协议
	IP_HEADER* SetIP(IP_TYPE type)
	{
		ARP = NULL;
		IP = (IP_HEADER*)Eth->Next();
		IP->Version = 4;
		IP->Length = sizeof(IP_HEADER) >> 2;
		IP->Protocol = type;
		Eth->Type = ETH_IP;

		return IP;
	}

	ICMP_HEADER* SetICMP()
	{
		SetIP(IP_ICMP);

		TCP = NULL;
		UDP = NULL;
		ICMP = (ICMP_HEADER*)IP->Next();

		Payload = ICMP->Next();
		PayloadLength = 0;

		return ICMP;
	}

	// 设置使用UDP协议
	UDP_HEADER* SetUDP()
	{
		SetIP(IP_UDP);

		ICMP = NULL;
		TCP = NULL;
		UDP = (UDP_HEADER*)IP->Next();

		UDP->Length = sizeof(UDP_HEADER);
		Payload = UDP->Next();
		PayloadLength = 0;

		return UDP;
	}
};

#endif
