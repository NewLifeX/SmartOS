#ifndef _TinyIP_DHCP_H_
#define _TinyIP_DHCP_H_

#include "Udp.h"
/*
// DHCP协议
class Dhcp : public UdpSocket
{
private:
	uint dhcpid;
	uint taskID;
	ulong _expiredTime;
	ulong _nextTime;

	void Discover(DHCP_HEADER& dhcp);
	void Request(DHCP_HEADER& dhcp);
	void PareOption(Stream& bs);

	void SendDhcp(DHCP_HEADER& dhcp, uint len);

	static void SendDiscover(void* param);
public:
	bool Running;	// 正在运行
	bool Result;	// 是否获取IP成功
	uint ExpiredTime;	// 过期时间

	Dhcp(TinyIP* tip);

	void Start();	// 开始
	void Stop();	// 停止

	EventHandler OnStop;

protected:
	virtual void OnProcess(IP_HEADER& ip, UDP_HEADER& udp, Stream& ms);
};
*/





class Dhcp
{
	Dhcp(W5500 * host);
public:
	DHCP_HEADER * dhcpdata;

private:
	W5500 * _w5500;
	UDPClient * _UdpPort;
	
	uint dhcpid;
	uint taskID;
	ulong _expiredTime;
	ulong _nextTime;

	void Discover(DHCP_HEADER& dhcp);
	void Request(DHCP_HEADER& dhcp);
	void PareOption(Stream& bs);

	void SendDhcp(DHCP_HEADER& dhcp, uint len);

	static void SendDiscover(void* param);
public:
	bool Running;	// 正在运行
	bool Result;	// 是否获取IP成功
	uint ExpiredTime;	// 过期时间

	Dhcp(TinyIP* tip);

	void Start();	// 开始
	void Stop();	// 停止

	EventHandler OnStop;

protected:
	virtual void OnProcess(IP_HEADER& ip, UDP_HEADER& udp, Stream& ms);
};

#endif
