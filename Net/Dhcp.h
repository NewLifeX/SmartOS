#ifndef _TinyIP_DHCP_H_
#define _TinyIP_DHCP_H_

#include "Net.h"

// DHCP协议
class Dhcp
{
private:
	uint dhcpid;
	uint taskID;
	ulong _expiredTime;
	ulong _nextTime;

	void Discover();
	void Request();
	void PareOption(Stream& ms);

	void SendDhcp(byte* buf, uint len);

	static void SendDiscover(void* param);
public:
	ISocket*	Socket;
	ISocketHost*	Host;	// 主机

	bool Running;	// 正在运行
	bool Result;	// 是否获取IP成功
	uint ExpiredTime;	// 过期时间

	Dhcp(ISocket* socket);
	~Dhcp();

	void Start();	// 开始
	void Stop();	// 停止

	EventHandler OnStop;

private:
	static uint OnReceive(ITransport* port, byte* buf, uint len, void* param);
	void Process(ByteArray& bs);
};

#endif
