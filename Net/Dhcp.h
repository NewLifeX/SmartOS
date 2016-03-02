#ifndef _TinyIP_DHCP_H_
#define _TinyIP_DHCP_H_

#include "Net.h"

// DHCP协议
class Dhcp
{
private:
	uint dhcpid;	// 事务ID
	uint taskID;	// 任务ID
	ulong _expired;	// 目标过期时间，毫秒

	void Discover();
	void Request();
	void PareOption(Stream& ms);

	void SendDhcp(byte* buf, uint len);

	static void Loop(void* param);
public:
	ISocket*	Socket;
	ISocketHost*	Host;	// 主机
	IPAddress	IP;			// 获取的IP地址

	uint ExpiredTime;	// 过期时间，默认10000毫秒
	bool Running;	// 正在运行
	bool Result;	// 是否获取IP成功
	byte Times;		// 运行次数
	byte MaxTimes;	// 最大重试次数，默认10次，超过该次数仍然失败则重启系统

	Dhcp(ISocket* socket);
	~Dhcp();

	void Start();	// 开始
	void Stop();	// 停止

	EventHandler OnStop;

private:
	static uint OnReceive(ITransport* port, Array& bs, void* param, void* param2);
	void Process(Array& bs, const IPEndPoint& ep);
};

#endif
