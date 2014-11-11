#include "Dhcp.h"

#define NET_DEBUG DEBUG

Dhcp::Dhcp(TinyIP* tip) : UdpSocket(tip)
{
	Type = IP_UDP;
	Port = 68;

	Running = false;
	Result = false;

	OnStop = NULL;
}

void Dhcp::SendDhcp(DHCP_HEADER* dhcp, uint len)
{
	byte* p = dhcp->Next();
	if(p[len - 1] != DHCP_OPT_End)
	{
		// 此时指向的是负载数据后的第一个字节，所以第一个opt不许Next
		DHCP_OPT* opt = (DHCP_OPT*)(p + len);
		opt = opt->SetClientId((byte*)&Tip->Mac, 6);
		opt = opt->Next()->SetData(DHCP_OPT_RequestedIP, Tip->IP);
		opt = opt->Next()->SetData(DHCP_OPT_HostName, (byte*)"YWS_SmartOS", 11);
		opt = opt->Next()->SetData(DHCP_OPT_Vendor, (byte*)"http://www.NewLifeX.com", 23);
		byte ps[] = { 0x01, 0x06, 0x03, 0x2b}; // 需要参数 Mask/DNS/Router/Vendor
		opt = opt->Next()->SetData(DHCP_OPT_ParameterList, ps, ArrayLength(ps));
		opt = opt->Next()->End();

		len = (byte*)opt + 1 - p;
	}

	memcpy(dhcp->ClientMac, (byte*)&Tip->Mac, 6);
	//dhcp->ClientMac = Tip->Mac;

	Tip->RemoteMac = 0xFFFFFFFFFFFFFFFF;
	Tip->RemoteIP = 0xFFFFFFFF;
	Tip->Port = 68;
	Tip->RemotePort = 67;

	// 如果最后一个字节不是DHCP_OPT_End，则增加End
	//byte* p = (byte*)dhcp + sizeof(DHCP_HEADER);
	//if(p[len - 1] != DHCP_OPT_End) p[len++] = DHCP_OPT_End;

	Send((byte*)dhcp, sizeof(DHCP_HEADER) + len, false);
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

// 设置选项
void SetOption(byte* p, int len)
{
}

// 找服务器
void Dhcp::Discover(DHCP_HEADER* dhcp)
{
	byte* p = dhcp->Next();
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Discover);

	//Tip->RemoteMac = 0xFFFFFFFFFFFFFFFF;
	//Tip->RemoteIP = 0xFFFFFFFF;
	SendDhcp(dhcp, (byte*)opt->Next() - p);
}

void Dhcp::Request(DHCP_HEADER* dhcp)
{
	byte* p = dhcp->Next();
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Request);
	opt = opt->Next()->SetData(DHCP_OPT_DHCPServer, Tip->DHCPServer);

	// 发往DHCP服务器
	//Tip->RemoteMac = 0xFFFFFFFFFFFFFFFF;
	//Tip->RemoteIP = Tip->DHCPServer;
	SendDhcp(dhcp, (byte*)opt->Next() - p);
}

void Dhcp::PareOption(byte* buf, uint len)
{
	byte* p = buf;
	byte* end = p + len;
	while(p < end)
	{
		byte opt = *p++;
		if(opt == DHCP_OPT_End) break;
		byte len = *p++;
		// 有些家用路由器会发送错误的len，大于4字节，导致覆盖前后数据
		switch(opt)
		{
			case DHCP_OPT_Mask: Tip->Mask = *(uint*)p; break;
			case DHCP_OPT_DNSServer: Tip->DNSServer = *(uint*)p; break;
			case DHCP_OPT_Router: Tip->Gateway = *(uint*)p; break;
			case DHCP_OPT_DHCPServer: Tip->DHCPServer = *(uint*)p; break;
#if NET_DEBUG
			//default:
			//	debug_printf("Unkown DHCP Option=%d Length=%d\r\n", opt, len);
#endif
		}
		p += len;
	}
}

void RenewDHCP(void* param)
{
	TinyIP* tip = (TinyIP*)param;
	if(tip)
	{
		/*Dhcp dhcp(tip);
		dhcp.Start();*/
		// 不能使用栈分配，因为是异步操作
		Dhcp* dhcp = new Dhcp(tip);
		dhcp->Start();
		/*if(!dhcp.Start())
		{
			debug_printf("TinyIP DHCP Fail!\r\n\r\n");
			return;
		}*/
	}
}

void Dhcp::Start()
{
	int s = 10;
	_expiredTime = Time.Current() + s * 1000000;
	dhcpid = (uint)Time.Current();

	debug_printf("Dhcp::Start ExpiredTime=%ds DhcpID=0x%08x\r\n", s, dhcpid);

	// 创建任务，每秒发送一次Discover
	debug_printf("Dhcp发送Discover ");
	taskID = Sys.AddTask(SendDiscover, this, 0, 1000000);

	// 通过DHCP获取IP期间，关闭Arp响应
	Tip->EnableArp = false;

	Running = true;
}

void Dhcp::Stop()
{
	Running = false;
	if(taskID)
	{
		debug_printf("Dhcp发送Discover ");
		Sys.RemoveTask(taskID);
	}
	taskID = 0;

	// 通过DHCP获取IP期间，关闭Arp响应
	Tip->EnableArp = true;

	debug_printf("Dhcp::Stop Result=%d DhcpID=0x%08x\r\n", Result, dhcpid);

	if(Result) Tip->ShowInfo();

	if(OnStop) OnStop(this, NULL);

	// 销毁自己
	delete this;
}

void Dhcp::SendDiscover(void* param)
{
	Dhcp* _dhcp = (Dhcp*)param;
	if(!_dhcp->Running) return;

	// 检查总等待时间
	if(_dhcp->_expiredTime < Time.Current())
	{
		_dhcp->Stop();
		return;
	}

	byte buf[400];
	//uint bufSize = ArrayLength(buf);

	ETH_HEADER*  eth  = (ETH_HEADER*) buf;
	IP_HEADER*   ip   = (IP_HEADER*)  eth->Next();
	UDP_HEADER*  udp  = (UDP_HEADER*) ip->Next();
	DHCP_HEADER* dhcp = (DHCP_HEADER*)udp->Next();

	// 向DHCP服务器广播
	debug_printf("DHCP::Discover...\r\n");
	dhcp->Init(_dhcp->dhcpid, true);
	_dhcp->Discover(dhcp);
}

void Dhcp::OnReceive(UDP_HEADER* udp, MemoryStream& ms)
{
	DHCP_HEADER* dhcp = (DHCP_HEADER*)udp->Next();
	if(!dhcp->Valid()) return;

	byte* data = dhcp->Next();
	uint len = ms.Remain();

	// 获取DHCP消息类型
	DHCP_OPT* opt = GetOption(data, len, DHCP_OPT_MessageType);
	if(!opt) return;

	if(opt->Data == DHCP_TYPE_Offer)
	{
		if(__REV(dhcp->TransID) == dhcpid)
		{
			Tip->IP = dhcp->YourIP;
			PareOption(dhcp->Next(), len);

			// 向网络宣告已经确认使用哪一个DHCP服务器提供的IP地址
			// 这里其实还应该发送ARP包确认IP是否被占用，如果被占用，还需要拒绝服务器提供的IP，比较复杂，可能性很低，暂时不考虑
#if NET_DEBUG
			debug_printf("DHCP::Offer IP:");
			TinyIP::ShowIP(dhcp->YourIP);
			debug_printf(" From ");
			TinyIP::ShowIP(Tip->RemoteIP);
			debug_printf("\r\n");
#endif

			dhcp->Init(dhcpid, true);
			Request(dhcp);
		}
	}
	else if(opt->Data == DHCP_TYPE_Ack)
	{
#if NET_DEBUG
		debug_printf("DHCP::Ack   IP:");
		TinyIP::ShowIP(dhcp->YourIP);
		debug_printf(" From ");
		TinyIP::ShowIP(Tip->RemoteIP);
		debug_printf("\r\n");
#endif

		//if(dhcp->YourIP == Tip->IP)
		{
			// 查找租约时间，提前续约
			opt = GetOption(data, len, DHCP_OPT_IPLeaseTime);
			if(opt)
			{
				// 续约时间，大字节序，时间单位秒
				uint time = __REV(*(uint*)&opt->Data);

				debug_printf("DHCP IPLeaseTime:%ds\r\n", time);

				// DHCP租约过了一半以后重新获取IP地址
				if(time > 0)
				{
					debug_printf("Dhcp过期获取 ");
					Sys.AddTask(RenewDHCP, Tip, (ulong)time / 2 * 1000000, -1);
				}
			}

			//return true;
			Result = true;
			// 完成任务
			Stop();
		}
	}
#if NET_DEBUG
	else if(opt->Data == DHCP_TYPE_Nak)
	{
		// 导致Nak的原因
		opt = GetOption(data, len, DHCP_OPT_Message);
		debug_printf("DHCP::Nak   IP:");
		TinyIP::ShowIP(Tip->IP);
		debug_printf(" From ");
		TinyIP::ShowIP(Tip->RemoteIP);
		if(opt)
		{
			debug_printf(" ");
			Sys.ShowString(&opt->Data, opt->Length);
		}
		debug_printf("\r\n");
	}
	else
		debug_printf("DHCP::Unkown Type=%d\r\n", opt->Data);
#endif
}
