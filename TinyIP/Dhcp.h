#ifndef _TinyIP_DHCP_H_
#define _TinyIP_DHCP_H_

#include "Udp.h"

// DHCP协议
class Dhcp : UdpSocket
{
private:
	uint dhcpid;
	uint taskID;
	ulong _expiredTime;
	ulong _nextTime;

	void Discover(DHCP_HEADER* dhcp);
	void Request(DHCP_HEADER* dhcp);
	void PareOption(byte* buf, uint len);

	void SendDhcp(DHCP_HEADER* dhcp, uint len);

	static void SendDiscover(void* param);
public:
	bool Running;	// 正在运行

	Dhcp(TinyIP* tip) : UdpSocket(tip) { Type = IP_UDP; Port = 68; }

	void Start();	// 开始
	void Stop();	// 停止

protected:
	virtual void OnReceive(UDP_HEADER* udp, MemoryStream& ms);
};

#endif
