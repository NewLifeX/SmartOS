#include "Time.h"
#include "Stream.h"
#include "ITransport.h"

#include "Dhcp.h"
#include "Ethernet.h"

#define NET_DEBUG DEBUG

Dhcp::Dhcp(ISocket* socket)
{
	Socket	= socket;
	Host	= socket->Host;

	socket->Local.Port		= 68;
	socket->Remote.Port		= 67;
	socket->Remote.Address	= IPAddress::Broadcast();

	Running	= false;
	Result	= false;
	ExpiredTime	= 10000;

	OnStop	= NULL;
	taskID	= 0;

	//ITransport* port = (ITransport*)socket;
	ITransport* port = dynamic_cast<ITransport*>(Socket);
	port->Register(OnReceive, this);
}

Dhcp::~Dhcp()
{
	Sys.RemoveTask(taskID);
}

void Dhcp::SendDhcp(byte* buf, uint len)
{
	DHCP_HEADER* dhcp = (DHCP_HEADER*)buf;
	byte* p = dhcp->Next();
	if(p[len - 1] != DHCP_OPT_End)
	{
		// 此时指向的是负载数据后的第一个字节，所以第一个opt不许Next
		DHCP_OPT* opt = (DHCP_OPT*)(p + len);
		opt = opt->SetClientId(Host->Mac);
		if(!Host->IP.IsAny())
			opt = opt->Next()->SetData(DHCP_OPT_RequestedIP, Host->IP.Value);

		// 构造产品名称，把ID第一个字节附到最后
		String name;
		name += "WSWL_SmartOS_";	// 产生拷贝效果
		name.Append(Sys.ID[0]);

		opt = opt->Next()->SetData(DHCP_OPT_HostName, name);
		String vendor = "http://www.NewLifeX.com";
		opt = opt->Next()->SetData(DHCP_OPT_Vendor, vendor);
		byte ps[] = { 0x01, 0x06, 0x03, 0x2b}; // 需要参数 Mask/DNS/Router/Vendor
		ByteArray bs(ps, ArrayLength(ps));
		opt = opt->Next()->SetData(DHCP_OPT_ParameterList, bs);
		opt = opt->Next()->End();

		len = (byte*)opt + 1 - p;
	}

	for(int i=0; i<6; i++)
		dhcp->ClientMac[i] = Host->Mac[i];

	//Send(*dhcp.Prev(), sizeof(DHCP_HEADER) + len, Remote.Address, Remote.Port, false);
	ByteArray bs((byte*)dhcp, sizeof(DHCP_HEADER) + len);
	Socket->Send(bs);
}

// 找服务器
void Dhcp::Discover()
{
	byte buf[sizeof(DHCP_HEADER) + 200];
	DHCP_HEADER* dhcp = (DHCP_HEADER*)buf;

	debug_printf("DHCP::Discover...\r\n");
	dhcp->Init(dhcpid, false);

	byte* p = dhcp->Next();
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Discover);

	SendDhcp(buf, (byte*)opt->Next() - p);
}

void Dhcp::Request()
{
	byte buf[sizeof(DHCP_HEADER) + 200];
	DHCP_HEADER* dhcp = (DHCP_HEADER*)buf;

	debug_printf("DHCP::Request...\r\n");
	dhcp->Init(dhcpid, false);

	byte* p = dhcp->Next();
	DHCP_OPT* opt = (DHCP_OPT*)p;
	opt->SetType(DHCP_TYPE_Request);

	opt = opt->Next()->SetData(DHCP_OPT_DHCPServer, Host->DHCPServer.Value);

	// 发往DHCP服务器
	SendDhcp(buf, (byte*)opt->Next() - p);
}

void Dhcp::Start()
{
	ulong now = Time.Current();
	_expiredTime = now + ExpiredTime;
	if(!dhcpid) dhcpid = (now << 16) | Time.CurrentTicks();

	debug_printf("Dhcp::Start ExpiredTime=%ds DhcpID=0x%08x\r\n", ExpiredTime, dhcpid);

	ITransport* port = dynamic_cast<ITransport*>(Socket);
	if(port) port->Open();

	// 创建任务，每秒发送一次Discover
	if(!taskID)
		taskID = Sys.AddTask(SendDiscover, this, 0, 1000, "DHCP获取");
	else
	{
		Sys.SetTaskPeriod(taskID, 1000);
		Sys.SetTask(taskID, true);
	}

	Running = true;
}

void Dhcp::Stop()
{
	ITransport* port = dynamic_cast<ITransport*>(Socket);
	if(port) port->Close();

	Running	= false;
	Sys.SetTask(taskID, false);

	debug_printf("Dhcp::Stop Result=%d DhcpID=0x%08x\r\n", Result, dhcpid);

	if(OnStop) OnStop(this, NULL);
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

	// 向DHCP服务器广播
	_dhcp->Discover();
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
	while(ms.Remain())
	{
		byte opt = ms.Read<byte>();
		if(opt == DHCP_OPT_End) break;
		byte len = ms.Read<byte>();
		// 有些家用路由器会发送错误的len，大于4字节，导致覆盖前后数据
		switch(opt)
		{
			case DHCP_OPT_Mask:			Host->Mask		= ms.Read<int>(); len -= 4; break;
			case DHCP_OPT_DNSServer:	Host->DNSServer	= ms.Read<int>(); len -= 4; break;
			case DHCP_OPT_Router:		Host->Gateway	= ms.Read<int>(); len -= 4; break;
			case DHCP_OPT_DHCPServer:	Host->DHCPServer	= ms.Read<int>(); len -= 4; break;
			//default:
			//	debug_printf("Unkown DHCP Option=%d Length=%d\r\n", opt, len);
		}
		// DNS可能有多个IP，就不止4长度了
		if(len > 0) ms.Seek(len);
	}
}

uint Dhcp::OnReceive(ITransport* port, ByteArray& bs, void* param, void* param2)
{
	/*IPEndPoint* ep = (IPEndPoint*)param2;
	if(ep)
	{
		debug_printf("收到数据，来自 ");
		ep->Show(true);
	}*/
	
	((Dhcp*)param)->Process(bs);

	return 0;
}

void Dhcp::Process(ByteArray& bs)
{
	DHCP_HEADER* dhcp = (DHCP_HEADER*)bs.GetBuffer();
	if(!dhcp->Valid()) return;

	byte* data	= dhcp->Next();
	uint len	= bs.Length() - sizeof(DHCP_HEADER);

	// 获取DHCP消息类型
	DHCP_OPT* opt = GetOption(data, len, DHCP_OPT_MessageType);
	if(!opt) return;

	// 所有响应都需要检查事务ID
	if(__REV(dhcp->TransID) != dhcpid) return;

#if NET_DEBUG
	IPAddress& remote	= Socket->Remote.Address;
#endif

	if(opt->Data == DHCP_TYPE_Offer)
	{
		Host->IP = dhcp->YourIP;
		Stream optData(dhcp->Next(), len);
		PareOption(optData);

		// 向网络宣告已经确认使用哪一个DHCP服务器提供的IP地址
		// 这里其实还应该发送ARP包确认IP是否被占用，
		// 如果被占用，还需要拒绝服务器提供的IP，比较复杂，可能性很低，暂时不考虑
#if NET_DEBUG
		debug_printf("DHCP::Offer IP:");
		Host->IP.Show();
		debug_printf(" From ");
		remote.Show();
		debug_printf("\r\n");
#endif

		Request();
	}
	else if(opt->Data == DHCP_TYPE_Ack)
	{
		Host->IP = dhcp->YourIP;
#if NET_DEBUG
		debug_printf("DHCP::Ack   IP:");
		IPAddress(dhcp->YourIP).Show();
		debug_printf(" From ");
		remote.Show();
		debug_printf("\r\n");
#endif

		Result = true;
		// 完成任务
		Stop();

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
				Sys.SetTaskPeriod(taskID, (ulong)time / 2 * 1000);
				Sys.SetTask(taskID, true);
			}
		}
	}
#if NET_DEBUG
	else if(opt->Data == DHCP_TYPE_Nak)
	{
		// 导致Nak的原因
		opt = GetOption(data, len, DHCP_OPT_Message);
		debug_printf("DHCP::Nak   IP:");
		Host->IP.Show();
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
