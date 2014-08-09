#ifndef _Net_H_
#define _Net_H_

//#include "Sys.h"

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

//Mac头部，总长度14字节
typedef struct _ETH_HEADER
{
	unsigned char DestMac[6]; //目标mac地址
	unsigned char SrcMac[6]; //源mac地址
	ETH_TYPE Type; //以太网类型
}ETH_HEADER;

// IP协议类型
typedef enum
{
	IP_ICMP = 1,
	IP_TCP = 6,
	IP_UDP = 17,
}IP_TYPE;

//IP头部，总长度20字节
typedef struct _IP_HEADER
{
	#if LITTLE_ENDIAN
	unsigned char Length:4;  //首部长度
	unsigned char Version:4; //版本
	#else
	unsigned char Version:4; //版本
	unsigned char Length:4;  //首部长度
	#endif
	unsigned char ToS;       //服务类型
	unsigned short TotalLength;	//总长度
	unsigned short Identifier;	//标志
	unsigned char Flags;		// 标识是否对数据包进行分段
	unsigned char FragmentOffset;	// 记录分段的偏移值。接收者会根据这个值进行数据包的重新组和
	unsigned char TTL;			//生存时间
	IP_TYPE Protocol;		//协议
	unsigned short Checksum;	//检验和
	unsigned char SrcIP[4];		//源IP地址
	unsigned char DestIP[4];	//目的IP地址
}IP_HEADER;

//TCP头部，总长度20字节
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
}TCP_HEADER;

//UDP头部，总长度8字节
typedef struct _UDP_HEADER
{
	unsigned short SrcPort; //远端口号
	unsigned short DestPort; //目的端口号
	unsigned short Length;      //udp头部长度
	unsigned short Checksum;  //16位udp检验和
}UDP_HEADER;

//ICMP头部，总长度4字节
typedef struct _ICMP_HEADER
{
	unsigned char Type;   //类型
	unsigned char Code;        //代码
	unsigned short Checksum;    //16位检验和
}ICMP_HEADER;

// ARP头部
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
}ARP_HEADER;

/*********************************************/
//ICMP的各种形式
//icmpx,x==icmp_type;
//icmp报文(能到达目的地,响应-请求包)
struct icmp8
{
	unsigned char icmp_type; //type of message(报文类型)
	unsigned char icmp_code; //type sub code(报文类型子码)
	unsigned short icmp_cksum;
	unsigned short icmp_id;
	unsigned short icmp_seq;
	char icmp_data[1];
};
//icmp报文(能返回目的地,响应-应答包)
struct icmp0
{
	unsigned char icmp_type; //type of message(报文类型)
	unsigned char icmp_code; //type sub code(报文类型子码)
	unsigned short icmp_cksum;
	unsigned short icmp_id;
	unsigned short icmp_seq;
	char icmp_data[1];
};
//icmp报文(不能到达目的地)
struct icmp3
{
	unsigned char icmp_type; //type of message(报文类型)
	unsigned char icmp_code; //type sub code(报文类型子码),例如:0网络原因不能到达,1主机原因不能到达...
	unsigned short icmp_cksum;
	unsigned short icmp_pmvoid;
	unsigned short icmp_nextmtu;
	char icmp_data[1];
};
//icmp报文(重发结构体)
struct icmp5
{
	unsigned char icmp_type; //type of message(报文类型)
	unsigned char icmp_code; //type sub code(报文类型子码)
	unsigned short icmp_cksum;
	unsigned int icmp_gwaddr;
	char icmp_data[1];
};
struct icmp11
{
	unsigned char icmp_type; //type of message(报文类型)
	unsigned char icmp_code; //type sub code(报文类型子码)
	unsigned short icmp_cksum;
	unsigned int icmp_void;
	char icmp_data[1];
};

#endif
