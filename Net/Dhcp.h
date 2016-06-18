#ifndef _TinyIP_DHCP_H_
#define _TinyIP_DHCP_H_

// DHCP协议
class Dhcp
{
private:
	uint dhcpid;	// 事务ID
	uint taskID;	// 任务ID
	UInt64 _expired;	// 目标过期时间，毫秒
	ISocket*	Socket;

	void Discover();
	void Request();
	void PareOption(Stream& ms);

	void SendDhcp(byte* buf, uint len);

	static void Loop(void* param);
public:
	ISocketHost&	Host;	// 主机
	IPAddress	IP;			// 获取的IP地址

	uint ExpiredTime;	// 过期时间，默认5000毫秒
	bool Running;	// 正在运行
	bool Result;	// 是否获取IP成功
	byte Times;		// 运行次数
	byte MaxTimes;	// 最大重试次数，默认6次，超过该次数仍然失败则恢复上一次设置

	Dhcp(ISocketHost& host);
	~Dhcp();

	void Start();	// 开始
	void Stop();	// 停止

	Delegate<Dhcp&>	OnStop;	// 带this参数

private:
	static uint OnReceive(ITransport* port, Buffer& bs, void* param, void* param2);
	void Process(Buffer& bs, const IPEndPoint& ep);
};

#endif
