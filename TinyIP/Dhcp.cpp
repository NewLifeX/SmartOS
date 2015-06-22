﻿#include "Dhcp.h"

#define NET_DEBUG DEBUG

Dhcp::Dhcp(TinyIP* tip) : UdpSocket(tip)
{
	Type = IP_UDP;
	Port = 68;
	RemotePort = 67;

	Running = false;
	Result = false;
	ExpiredTime = 10;

	OnStop = NULL;
}

void Dhcp::SendDhcp(DHCP_HEADER& dhcp, uint len)
{
	byte* p = dhcp.Next();
	if(p[len - 1] != DHCP_OPT_End)
	{
		// 此时指向的是负载数据后的第一个字节，所以第一个opt不许Next
		DHCP_OPT* opt = (DHCP_OPT*)(p + len);
		opt = opt->SetClientId(Tip->Mac);
		if(!Tip->IP.IsAny())
			opt = opt->Next()->SetData(DHCP_OPT_RequestedIP, Tip->IP.Value);

		// 构造产品名称，把ID第一个字节附到最后
		String name;
		name.Format("WSWL_SmartOS_%02X", Sys.ID[0]);

		opt = opt->Next()->SetData(DHCP_OPT_HostName, name);
		String vendor = "http://www.NewLifeX.com";
		opt = opt->Next()->SetData(DHCP_OPT_Vendor, vendor);
		byte ps[] = { 0x01, 0x06, 0x03, 0x2b}; // 需要参数 Mask/DNS/Router/Vendor
		ByteArray bs(ps, ArrayLength(ps));
		opt = opt->Next()->SetData(DHCP_OPT_ParameterList, bs);
		opt = opt->Next()->End();

		len = (byte*)opt + 1 - p;
	}

	//memcpy(dhcp->ClientMac, (byte*)&Tip->Mac.Value, 6);
	for(int i=0; i<6; i++)
		dhcp.ClientMac[i] = Tip->Mac[i];

	RemoteIP = IPAddress::Broadcast;

	Send(*dhcp.Prev(), sizeof(DHCP_HEADER) + len, false);
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

// 找服务器
void Dhcp::Discover(DHCP_HEADER& dhcp)
{
	byte* p = dhcp.Next();
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Discover);

	SendDhcp(dhcp, (byte*)opt->Next() - p);
}

void Dhcp::Request(DHCP_HEADER& dhcp)
{
	byte* p = dhcp.Next();
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Request);

	opt = opt->Next()->SetData(DHCP_OPT_DHCPServer, Tip->DHCPServer.Value);

	// 发往DHCP服务器
	SendDhcp(dhcp, (byte*)opt->Next() - p);
}

void Dhcp::PareOption(Stream& ms)
{
	while(ms.Remain())
	{
		byte opt = ms.Read<byte>();
		if(opt == DHCP_OPT_End) break;
		byte len = ms.Read<byte>();
		// 有些家用路由器会发送错误的len，大于4字节，导致覆盖前后数据
		switch(opt)
		{
			case DHCP_OPT_Mask:			Tip->Mask		= ms.Read<int>(); break;
			case DHCP_OPT_DNSServer:	Tip->DNSServer	= ms.Read<int>(); break;
			case DHCP_OPT_Router:		Tip->Gateway	= ms.Read<int>(); break;
			case DHCP_OPT_DHCPServer:	Tip->DHCPServer	= ms.Read<int>(); break;
#if NET_DEBUG
			//default:
			//	debug_printf("Unkown DHCP Option=%d Length=%d\r\n", opt, len);
#endif
		}
		ms.Seek(len - 4);
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
	}
}

void Dhcp::Start()
{
	_expiredTime = Time.Current() + ExpiredTime * 1000000;
	dhcpid = (uint)Time.Current();

	debug_printf("Dhcp::Start ExpiredTime=%ds DhcpID=0x%08x\r\n", ExpiredTime, dhcpid);

	// 创建任务，每秒发送一次Discover
	debug_printf("Dhcp发送Discover ");
	taskID = Sys.AddTask(SendDiscover, this, 0, 1000000);

	// 通过DHCP获取IP期间，关闭Arp响应
	//Tip->EnableArp = false;
	if(Tip->Arp) Tip->Arp->Enable = false;

	Running = true;

	Open();
}

void Dhcp::Stop()
{
	Close();

	Running = false;
	if(taskID)
	{
		debug_printf("Dhcp发送Discover ");
		Sys.RemoveTask(taskID);
	}
	taskID = 0;

	// 通过DHCP获取IP期间，关闭Arp响应
	//Tip->EnableArp = true;
	if(Tip->Arp) Tip->Arp->Enable = true;

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

	Stream ms(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER) + sizeof(DHCP_HEADER) + 100);
	ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER));

	DHCP_HEADER* dhcp = ms.Retrieve<DHCP_HEADER>();

	// 向DHCP服务器广播
	debug_printf("DHCP::Discover...\r\n");
	dhcp->Init(_dhcp->dhcpid, true);
	_dhcp->Discover(*dhcp);
}

void Dhcp::OnProcess(IP_HEADER& ip, UDP_HEADER& udp, Stream& ms)
{
	DHCP_HEADER* dhcp = (DHCP_HEADER*)udp.Next();
	if(!dhcp->Valid()) return;

	byte* data = dhcp->Next();
	uint len = ms.Remain();

	// 获取DHCP消息类型
	DHCP_OPT* opt = GetOption(data, len, DHCP_OPT_MessageType);
	if(!opt) return;

	// 所有响应都需要检查事务ID
	if(__REV(dhcp->TransID) != dhcpid) return;

	IPAddress remote = ip.SrcIP;
	
	if(opt->Data == DHCP_TYPE_Offer)
	{
		Tip->IP = dhcp->YourIP;
		Stream optData(dhcp->Next(), len);
		PareOption(optData);

		// 向网络宣告已经确认使用哪一个DHCP服务器提供的IP地址
		// 这里其实还应该发送ARP包确认IP是否被占用，如果被占用，还需要拒绝服务器提供的IP，比较复杂，可能性很低，暂时不考虑
#if NET_DEBUG
		debug_printf("DHCP::Offer IP:");
		Tip->IP.Show();
		debug_printf(" From ");
		remote.Show();
		debug_printf("\r\n");
#endif

		// 独立分配缓冲区进行操作，避免数据被其它线程篡改
		//byte buf[400];
		Stream ms(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER) + sizeof(DHCP_HEADER) + 100);
		ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER));

		DHCP_HEADER* dhcp2 = ms.Retrieve<DHCP_HEADER>();

		dhcp2->Init(dhcpid, true);
		Request(*dhcp2);
	}
	else if(opt->Data == DHCP_TYPE_Ack)
	{
		Tip->IP = dhcp->YourIP;
#if NET_DEBUG
		debug_printf("DHCP::Ack   IP:");
		IPAddress(dhcp->YourIP).Show();
		debug_printf(" From ");
		remote.Show();
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
		Tip->IP.Show();
		debug_printf(" From ");
		remote.Show();
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
