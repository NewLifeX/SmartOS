#ifndef _TinyIP_ARP_H_
#define _TinyIP_ARP_H_

#include "TinyIP.h"

// ARP协议
class ArpSocket : public Socket
{
private:
	// ARP表
	typedef struct
	{
		IPAddr	IP;
		MacAddr	Mac;
		uint	Time;	// 生存时间，秒
	}ARP_ITEM;

	ARP_ITEM* _Arps;	// Arp表，动态分配

public:
	byte Count;	// Arp表行数，默认16行

	ArpSocket(TinyIP* tip);
	virtual ~ArpSocket();

	// 处理数据包
	virtual bool Process(IP_HEADER& ip, Stream& ms);

	// 请求Arp并返回其Mac。timeout超时3秒，如果没有超时时间，表示异步请求，不用等待结果
	bool Request(IPAddress& ip, MacAddress& mac, int timeout = 3);
	bool Resolve(IPAddress& ip, MacAddress& mac);
	void Add(IPAddress& ip, MacAddress& mac);
};

#endif
