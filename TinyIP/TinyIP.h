#ifndef _TinyIP_H_
#define _TinyIP_H_

#include "Enc28j60.h"
#include "Net/Ethernet.h"

// 默认打开所有模块，用户需要根据自己需要在编译器设置关闭条件，比如TinyIP_DHCP=0
#ifndef TinyIP_ICMP
	#define TinyIP_ICMP 1
#endif

#ifndef TinyIP_TCP
	#define TinyIP_TCP 1
#endif

#ifndef TinyIP_UDP
	#define TinyIP_UDP 1
#endif

#ifndef TinyIP_DHCP
	#define TinyIP_DHCP 1
#endif

/*#ifndef TinyIP_DNS
	#define TinyIP_DNS 1
#endif*/

// DHCP和DNS需要UDP
#if TinyIP_DHCP || TinyIP_DNS
	#undef TinyIP_UDP
	#define TinyIP_UDP 1
#endif

// 精简IP类
class TinyIP //: protected IEthernetAdapter
{
private:
    Enc28j60* _enc;
	NetPacker* _net;

	byte* Buffer; // 缓冲区

	static void Work(void* param);	// 任务函数
	void OnWork();	// 循环调度的任务

	void ProcessArp(byte* buf, uint len);
	void SendEthernet(ETH_TYPE type, byte* buf, uint len);
	void SendIP(IP_TYPE type, byte* buf, uint len);

#if TinyIP_ICMP
	void ProcessICMP(byte* buf, uint len);
#endif

#if TinyIP_TCP
	void ProcessTcp(byte* buf, uint len);
	void SendTcp(byte* buf, uint len, byte flags);

	byte seqnum;

	void TcpHead(uint ackNum, bool mss, bool cp_seq);
	void TcpAck(byte* buf, uint dlen);
#endif

#if TinyIP_UDP
	void ProcessUdp(byte* buf, uint len);
	void SendUdp(byte* buf, uint len, bool checksum = true);
#endif

	uint CheckSum(byte* buf, uint len, byte type);

#if TinyIP_DHCP
	uint dhcp_id;
	void DHCPDiscover();
	void DHCPRequest(byte* buf);
	void PareOption(byte* buf, int len);
	void DHCPConfig(byte* buf);

	void SendDhcp(byte* buf, uint len);
#endif

	/*virtual void Send(IP_TYPE type, uint len);
	// 获取负载数据指针。外部可以直接填充数据
	virtual byte* GetPayload();
	virtual void OnReceive(byte* buf, uint len);*/
public:
    byte IP[4];
    byte Mask[4];
	byte Mac[6];
	ushort Port;

	byte RemoteMac[6];
	byte RemoteIP[4];
	ushort RemotePort;

	ushort BufferSize;	// 缓冲区大小
	byte DHCPServer[4];
	byte DNSServer[4];
	byte Gateway[4];

#if TinyIP_DHCP
	bool IPIsReady;
	bool UseDHCP;
#endif

    TinyIP(Enc28j60* enc, byte ip[4] = NULL, byte mac[6] = NULL);
    virtual ~TinyIP();

	bool Init();
	static void ShowIP(byte* ip);
	static void ShowMac(byte* mac);
	static void ShowData(byte* buf, uint len);

#if TinyIP_ICMP
	// 收到Ping请求时触发，传递结构体和负载数据长度
	typedef void (*PingHandler)(TinyIP* tip, ICMP_HEADER* icmp, byte* buf, uint len);
	PingHandler OnPing;

	// Ping目的地址，附带a~z重复的负载数据
	void Ping(byte ip[4], uint payloadLength = 32);
#endif

#if TinyIP_UDP
	// 收到Udp数据时触发，传递结构体和负载数据长度
	typedef void (*UdpHandler)(TinyIP* tip, UDP_HEADER* udp, byte* buf, uint len);
	UdpHandler OnUdpReceived;

	// 发送UDP数据到目标地址
	void SendUdp(byte* buf, uint len, byte ip[4], ushort port);
#endif

#if TinyIP_TCP
	bool TcpConnect(byte ip[4], ushort port);	// 连接远程
    void TcpSend(byte* packet, uint len);
    void TcpClose(byte* packet, uint maxlen);

	// 收到Tcp数据时触发，传递结构体和负载数据长度
	typedef void (*TcpHandler)(TinyIP* tip, TCP_HEADER* tcp, byte* buf, uint len);
	TcpHandler OnTcpAccepted;
	TcpHandler OnTcpReceived;
	TcpHandler OnTcpDisconnected;
#endif
};

#endif
