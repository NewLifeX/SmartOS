#include "Sys.h"
#include "ITransport.h"

#include "Socket.h"
#include "NetworkInterface.h"

#include "Dhcp.h"
#include "Ethernet.h"
#include "Core\Random.h"
#include "Kernel\TTime.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

Dhcp::Dhcp(NetworkInterface& host) : Host(host)
{
	_Socket	= host.CreateSocket(NetType::Udp);

	_Socket->Local.Port		= 68;
	_Socket->Remote.Port		= 67;
	_Socket->Remote.Address	= IPAddress::Broadcast();

	IP		= host.IP;

	Running	= false;
	Result	= false;
	Times	= 0;
	MaxTimes	= 6;
	ExpiredTime	= 500 * 10;

	taskID	= 0;

	auto port = dynamic_cast<ITransport*>(_Socket);
	port->Register(OnReceive, this);
}

Dhcp::~Dhcp()
{
	Sys.RemoveTask(taskID);
	delete _Socket;
}

void Dhcp::SendDhcp(byte* buf, uint len)
{
	if(!Running) return;

	auto dhcp	= (DHCP_HEADER*)buf;
	auto p		= dhcp->Next();
	if(p[len - 1] != DHCP_OPT_End)
	{
		// 此时指向的是负载数据后的第一个字节，所以第一个opt不许Next
		auto opt	= (DHCP_OPT*)(p + len);
		opt = opt->SetClientId(Host.Mac);
		if(!IP.IsAny())
			opt	= opt->Next()->SetData(DHCP_OPT_RequestedIP, IP.Value);

		// 构造产品名称，把ID第一个字节附到最后
		String name	= "SmartOS_";
		//name.Concat(Sys.ID[0], 16);
		//name.Concat(Sys.ID[1], 16);
		name	+= Buffer(Sys.ID, 2).ToHex();

		opt = opt->Next()->SetData(DHCP_OPT_HostName, name);
		String vendor = "www.wslink.cn";
		opt = opt->Next()->SetData(DHCP_OPT_Vendor, vendor);
		byte ps[] = { 0x01, 0x06, 0x03, 0x2b}; // 需要参数 Mask/DNS/Router/Vendor
		opt = opt->Next()->SetData(DHCP_OPT_ParameterList, Buffer(ps, sizeof(ps)));
		opt = opt->Next()->End();

		len = (byte*)opt + 1 - p;
	}

	Host.Mac.CopyTo(dhcp->ClientMac);

	Buffer bs(dhcp, sizeof(DHCP_HEADER) + len);
	_Socket->Send(bs);
}

// 找服务器
void Dhcp::Discover()
{
	if(!Running) return;

	byte buf[sizeof(DHCP_HEADER) + 200];
	auto dhcp	= (DHCP_HEADER*)buf;

	net_printf("DHCP::Discover...\r\n");
	//dhcpid	= Sys.Ms();
	dhcp->Init(dhcpid, false);

	auto p		= dhcp->Next();
	auto opt	= (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Discover);

	SendDhcp(buf, (byte*)opt->Next() - p);
}

void Dhcp::Request()
{
	if(!Running) return;

	byte buf[sizeof(DHCP_HEADER) + 200];
	auto dhcp	= (DHCP_HEADER*)buf;

	net_printf("DHCP::Request...\r\n");
	dhcp->Init(dhcpid, false);

	auto p		= dhcp->Next();
	auto opt	= (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Request);

	opt = opt->Next()->SetData(DHCP_OPT_DHCPServer, Server.Value);

	// 发往DHCP服务器
	SendDhcp(buf, (byte*)opt->Next() - p);
}

void Dhcp::Start()
{
	UInt64 now	= Sys.Ms();
	_expired	= now + ExpiredTime;
	Random ran(Time.CurrentTicks());
	uint randnum = (ran.Next()<<16) & 0xffff0000;
	dhcpid		= now + randnum;

	net_printf("Dhcp::Start ExpiredTime=%dms DhcpID=0x%08x\r\n", ExpiredTime, dhcpid);

	// 使用DHCP之前最好清空本地IP地址，KWF等软路由要求非常严格
	// 严格路由要求默认请求的IP必须在本网段，否则不予处理
	if(!IP.IsAny())
	{
		// 这里无法关闭主机，只能希望DHCP是第一个启动的Socket
		//Host->Close();
		Host.IP	= IP = IPAddress::Any();
		Host.Config();
	}

	// 发送网络请求时会自动开始
	//auto port = dynamic_cast<ITransport*>(_Socket);
	//if(port) port->Open();

	// 创建任务，每500ms发送一次Discover
	if(!taskID)
		taskID = Sys.AddTask(Loop, this, 0, 500, "DHCP获取");
	else
	{
		Sys.SetTaskPeriod(taskID, 500);
		Sys.SetTask(taskID, true);
	}

	Result	= false;
	Running = true;
}

void Dhcp::Stop()
{
	if(!Running) return;

	auto port = dynamic_cast<ITransport*>(_Socket);
	if(port) port->Close();

	Running	= false;
	Sys.SetTask(taskID, false);

	net_printf("Dhcp::Stop Result=%d DhcpID=0x%08x Times=%d MaxTimes=%d\r\n", Result, dhcpid, Times, MaxTimes);

	auto& host	= Host;
	if(Result)
	{
		// 成功后次数清零
		Times	= 0;

		// 获取IP成功，重新设置参数
		host.Config();
		host.SaveConfig();
	}
	else
	{
		// 如果未达到重试次数则重新开始
		if(++Times < MaxTimes)
		{
			Start();
		}
		else
		{
			net_printf("尝试次数 %d 超过最大允许次数 %d ，", Times, MaxTimes);

			// 恢复上一次设置，如果首次，则恢复出厂设置
			if(host.LoadConfig())
				net_printf("恢复上一次设置");
			else
			{
				net_printf("恢复出厂设置");
				host.InitConfig();
			}
			net_printf("\r\n");

			host.Config();

			// 3分钟后继续启动DHCP
			Sys.SetTaskPeriod(taskID, 3 * 60 * 1000);
			Sys.SetTask(taskID, true);
		}
	}

	//OnStop(*this);
	// 异步调用OnStop
	Sys.AddTask([](void* p){ auto d = (Dhcp*)p; d->OnStop(*d); }, this, 0, -1, "OnStop");
}

void Dhcp::Loop(void* param)
{
	auto& dhcp	= *(Dhcp*)param;
	// 网络未就绪时不要处理
	if(!dhcp.Host.Linked) return;

	if(!dhcp.Running)
	{
		// 上一次是成功的，这次定时任务可能就是重新获取IP
		if(dhcp.Result) dhcp.Start();
	}

	// 检查总等待时间
	if(dhcp._expired < Sys.Ms())
	{
		dhcp.Stop();
		return;
	}

	// 向DHCP服务器广播
	dhcp.Discover();
}

// 获取选项，返回数据部分指针
DHCP_OPT* GetOption(byte* p, int len, DHCP_OPTION option)
{
	byte* end = p + len;
	while(p < end)
	{
		byte opt = *p++;
		byte len = *p++;
		if(opt == DHCP_OPT_End) return 0;
		if(opt == option) return (DHCP_OPT*)(p - 2);

		p += len;
	}

	return 0;
}

void Dhcp::PareOption(Stream& ms)
{
	auto& host	= Host;
	while(ms.Remain())
	{
		byte opt = ms.ReadByte();
		if(opt == DHCP_OPT_End) break;
		byte len = ms.ReadByte();
		// 有些家用路由器会发送错误的len，大于4字节，导致覆盖前后数据
		switch(opt)
		{
			case DHCP_OPT_Mask:			host.Mask		= ms.ReadUInt32(); len -= 4; break;
			case DHCP_OPT_Router:		host.Gateway	= ms.ReadUInt32(); len -= 4; break;
			case DHCP_OPT_DHCPServer:	Server			= ms.ReadUInt32(); len -= 4; break;
			case DHCP_OPT_DNSServer:
			{
				// 有可能有多个DNS，只要第一个
				IPAddress dns	= ms.ReadUInt32();
				// 成功获取DHCP信息后，采用本地DNS为主DNS，阿里公共DNS为备用DNS
				IPAddress aliyun(223, 5, 5, 5);
				//if(host.DNSServer != dns) host.DNSServer2	= host.DNSServer;
				host.DNSServer2	= aliyun;
				host.DNSServer	= dns;
				len -= 4;
				break;
			}
			//default:
			//	net_printf("Unkown DHCP Option=%d Length=%d\r\n", opt, len);
		}
		// DNS可能有多个IP，就不止4长度了
		if(len > 0) ms.Seek(len);
	}
}

uint Dhcp::OnReceive(ITransport* port, Buffer& bs, void* param, void* param2)
{
	((Dhcp*)param)->Process(bs, *(const IPEndPoint*)param2);

	return 0;
}

void Dhcp::Process(Buffer& bs, const IPEndPoint& ep)
{
	auto dhcp	= (DHCP_HEADER*)bs.GetBuffer();
	if(!dhcp->Valid()) return;

	auto data	= dhcp->Next();
	uint len	= bs.Length() - sizeof(DHCP_HEADER);

	// 获取DHCP消息类型
	auto opt = GetOption(data, len, DHCP_OPT_MessageType);
	if(!opt) return;

	// 所有响应都需要检查事务ID
	if(_REV(dhcp->TransID) != dhcpid) return;

#if NET_DEBUG
	auto& remote	= ep.Address;
#endif

	if(opt->Data == DHCP_TYPE_Offer)
	{
		Host.IP = IP = dhcp->YourIP;
		Stream optData(dhcp->Next(), len);
		PareOption(optData);

		// 向网络宣告已经确认使用哪一个DHCP服务器提供的IP地址
		// 这里其实还应该发送ARP包确认IP是否被占用，
		// 如果被占用，还需要拒绝服务器提供的IP，比较复杂，可能性很低，暂时不考虑
#if NET_DEBUG
		net_printf("DHCP::Offer IP:");
		IP.Show();
		net_printf(" From ");
		remote.Show();
		net_printf("\r\n");
#endif

		Request();
	}
	else if(opt->Data == DHCP_TYPE_Ack)
	{
		Host.IP = IP = dhcp->YourIP;
#if NET_DEBUG
		net_printf("DHCP::Ack   IP:");
		IP.Show();
		net_printf(" From ");
		remote.Show();
		net_printf("\r\n");
#endif

		Result = true;
		// 完成任务
		Stop();

		// 查找租约时间，提前续约
		opt = GetOption(data, len, DHCP_OPT_IPLeaseTime);
		if(opt)
		{
			// 续约时间，大字节序，时间单位秒
			uint time = _REV(*(uint*)&opt->Data);

			net_printf("DHCP IPLeaseTime:%ds\r\n", time);

			// DHCP租约过期前提前一分钟重新获取IP地址
			if(time > 0)
			{
				Sys.SetTaskPeriod(taskID, (UInt64)(time - 60) * 1000);
				Sys.SetTask(taskID, true);
			}
		}
	}
#if NET_DEBUG
	else if(opt->Data == DHCP_TYPE_Nak)
	{
		// 导致Nak的原因
		opt = GetOption(data, len, DHCP_OPT_Message);
		net_printf("DHCP::Nak   IP:");
		IP.Show();
		net_printf(" From ");
		remote.Show();
		if(opt)
		{
			net_printf(" ");
			String((char*)&opt->Data, opt->Length).Show(true);
		}
		net_printf("\r\n");
	}
	else
		net_printf("DHCP::Unkown Type=%d\r\n", opt->Data);
#endif
}
