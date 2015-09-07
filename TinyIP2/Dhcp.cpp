#include "Time.h"
#include "Dhcp.h"

#define NET_DEBUG DEBUG


// 字节序
#ifndef LITTLE_ENDIAN
	#define LITTLE_ENDIAN   1
#endif

#pragma pack(push)	// 保存对齐状态
// 强制结构体紧凑分配空间
#pragma pack(1)


// DHCP头部，总长度240=0xF0字节，偏移42=0x2A，后面可选数据偏移282=0x11A
typedef struct _DHCP_HEADER
{
	byte	MsgType;		// 若是client送给server的封包，设为1，反向为2
	byte	HardType;		// 以太网1
	byte	HardLength;		// 以太网6
	byte	Hops;			// 若数据包需经过router传送，每站加1，若在同一网内，为0
	uint	TransID;		// 事务ID，是个随机数，用于客户和服务器之间匹配请求和相应消息
	ushort	Seconds;		// 由用户指定的时间，指开始地址获取和更新进行后的时间
	ushort	Flags;			// 从0-15bits，最左一bit为1时表示server将以广播方式传送封包给 client，其余尚未使用
	IPAddr	ClientIP;		// 客户机IP
	IPAddr	YourIP;			// 你的IP
	IPAddr	ServerIP;		// 服务器IP
	IPAddr	RelayIP;		// 中继代理IP
	byte	ClientMac[16];	// 客户端硬件地址
	byte	ServerName[64];	// 服务器名
	byte	BootFile[128];	// 启动文件名
	uint	Magic;			// 幻数0x63825363，小端0x63538263

	void Init(uint dhcpid, bool recursion = false)
	{
		// 为了安全，清空一次
		memset(this, 0, sizeof(this[0]));

		MsgType = 1;
		HardType = 1;
		HardLength = 6;
		Hops = 0;
		TransID = __REV(dhcpid);
		Flags = 0x80;	// 从0-15bits，最左一bit为1时表示server将以广播方式传送封包给 client，其余尚未使用
		SetMagic();

		// if(recursion) Prev()->Init(recursion);
	}

	uint Size()			{ return sizeof(this[0]); }
	// uint Offset()		{ return Prev()->Offset() + Size(); }
	// UDP_HEADER* Prev()	{ return (UDP_HEADER*)((byte*)this - sizeof(UDP_HEADER)); }
	byte* Next()		{ return (byte*)this + Size(); }

	void SetMagic()		{ Magic = 0x63538263; }
	bool Valid()		{ return Magic == 0x63538263; }
}DHCP_HEADER;

// DHCP后面可选数据格式为“代码+长度+数据”

typedef enum
{
	DHCP_OPT_Mask = 1,
	DHCP_OPT_Router = 3,
	DHCP_OPT_TimeServer = 4,
	DHCP_OPT_NameServer = 5,
	DHCP_OPT_DNSServer = 6,
	DHCP_OPT_LOGServer = 7,
	DHCP_OPT_HostName = 12,
	DHCP_OPT_MTU = 26,				// 0x1A
	DHCP_OPT_StaticRout = 33,		// 0x21
	DHCP_OPT_ARPCacheTimeout = 35,	// 0x23
	DHCP_OPT_NTPServer = 42,		// 0x2A
	DHCP_OPT_RequestedIP = 50,		// 0x32
	DHCP_OPT_IPLeaseTime = 51,		// 0x33
	DHCP_OPT_MessageType = 53,		// 0x35
	DHCP_OPT_DHCPServer = 54,		// 0x36
	DHCP_OPT_ParameterList = 55,	// 0x37
	DHCP_OPT_Message = 56,			// 0x38
	DHCP_OPT_MaxMessageSize = 57,	// 0x39
	DHCP_OPT_Vendor = 60,			// 0x3C
	DHCP_OPT_ClientIdentifier = 61,	// 0x3D
	DHCP_OPT_End = 255,
}DHCP_OPTION;

typedef enum
{
	DHCP_TYPE_Discover = 1,
	DHCP_TYPE_Offer = 2,
	DHCP_TYPE_Request = 3,
	DHCP_TYPE_Decline = 4,
	DHCP_TYPE_Ack = 5,
	DHCP_TYPE_Nak = 6,
	DHCP_TYPE_Release = 7,
	DHCP_TYPE_Inform = 8,
}DHCP_MSGTYPE;

typedef struct _DHCP_OPT
{
	DHCP_OPTION	Option;	// 代码
	byte		Length;	// 长度
	byte		Data;	// 数据

	struct _DHCP_OPT* Next() { return (struct _DHCP_OPT*)((byte*)this + 2 + Length); }

	struct _DHCP_OPT* SetType(DHCP_MSGTYPE type)
	{
		Option = DHCP_OPT_MessageType;
		Length = 1;
		Data = type;

		return this;
	}

	struct _DHCP_OPT* SetData(DHCP_OPTION option, ByteArray& bs)
	{
		Option = option;
		Length = bs.Length();
		bs.CopyTo(&Data);

		return this;
	}

	struct _DHCP_OPT* SetData(DHCP_OPTION option, String& str)
	{
		ByteArray bs(str);
		return SetData(option, bs);
	}

	struct _DHCP_OPT* SetData(DHCP_OPTION option, uint value)
	{
		Option = option;
		Length = 4;
		memcpy(&Data, (byte*)&value, Length);
		// 需要考虑地址对齐问题，只有4字节对齐，才可以直接使用整数赋值
		//*(uint*)&Data = value;

		return this;
	}

	struct _DHCP_OPT* SetClientId(MacAddress& mac)
	{
		Option = DHCP_OPT_ClientIdentifier;
		Length = 1 + 6;
		Data = 1;	// 类型ETHERNET=1
		memcpy(&Data + 1, &mac.Value, 6);

		return this;
	}

	struct _DHCP_OPT* End()
	{
		Option = DHCP_OPT_End;

		return this;
	}
}DHCP_OPT;

#pragma pack(pop)	// 恢复对齐状态


Dhcp::Dhcp(W5500 * host)
{
	assert_ptr(host);
	_w5500 = host;
	_w5500 -> = IPAddress(0, 0, 0, 0);
	_UdpPort = new UdpClient(host);
	
	_UdpPort->Local.Port = 68;
	_UdpPort->Remote.Port = 67;
	_UdpPort->Remote.Address = IPAddress::Broadcast();
	
	Running = false;
	Result = false;
	ExpiredTime = 10;

	OnStop = NULL;
}

//Dhcp::Dhcp(TinyIP* tip) : UdpSocket(tip)
//{
//	Type = IP_UDP;
//	Local.Port = 68;
//	Remote.Port = 67;
//	Remote.Address = IPAddress::Broadcast();
//
//	Running = false;
//	Result = false;
//	ExpiredTime = 10;
//
//	OnStop = NULL;
//}

void Dhcp::SendDhcp(DHCP_HEADER& dhcp, uint len)
{
	byte* p = dhcp.Next();
	if(p[len - 1] != DHCP_OPT_End)
	{
		// 此时指向的是负载数据后的第一个字节，所以第一个opt不许Next
		DHCP_OPT* opt = (DHCP_OPT*)(p + len);
		opt = opt->SetClientId(_w5500->Mac);
		if(!_w5500->IP.IsAny())
			opt = opt->Next()->SetData(DHCP_OPT_RequestedIP, _w5500->IP.Value);

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

	//memcpy(dhcp->ClientMac, (byte*)&_w5500->Mac.Value, 6);
	for(int i=0; i<6; i++)
		dhcp.ClientMac[i] = _w5500->Mac[i];

	//RemoteIP = IPAddress::Broadcast;

	Send(*dhcp.Prev(), sizeof(DHCP_HEADER) + len, Remote.Address, Remote.Port, false);
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

	opt = opt->Next()->SetData(DHCP_OPT_DHCPServer, _w5500->DHCPServer.Value);

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
			case DHCP_OPT_Mask:			_w5500->Mask		= ms.Read<int>(); len -= 4; break;
			case DHCP_OPT_DNSServer:	_w5500->DNSServer	= ms.Read<int>(); len -= 4; break;
			case DHCP_OPT_Router:		_w5500->Gateway	= ms.Read<int>(); len -= 4; break;
			case DHCP_OPT_DHCPServer:	_w5500->DHCPServer	= ms.Read<int>(); len -= 4; break;
			//default:
			//	debug_printf("Unkown DHCP Option=%d Length=%d\r\n", opt, len);
		}
		// DNS可能有多个IP，就不止4长度了
		if(len > 0) ms.Seek(len);
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
	//debug_printf("Dhcp发送Discover ");
	taskID = Sys.AddTask(SendDiscover, this, 0, 1000000, "DHCP");

	// 通过DHCP获取IP期间，关闭Arp响应
	//_w5500->EnableArp = false;
	if(_w5500->Arp) _w5500->Arp->Enable = false;

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
	//_w5500->EnableArp = true;
	if(_w5500->Arp) _w5500->Arp->Enable = true;

	debug_printf("Dhcp::Stop Result=%d DhcpID=0x%08x\r\n", Result, dhcpid);

	if(Result) _w5500->ShowInfo();

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

	byte buf[/*sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER) + */sizeof(DHCP_HEADER) + 100];
	Stream ms(buf, ArrayLength(buf));
	//ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER));

	DHCP_HEADER* dhcp = ms.Retrieve<DHCP_HEADER>();

	// 向DHCP服务器广播
	debug_printf("DHCP::Discover...\r\n");
	// dhcp->Init(_dhcp->dhcpid, true);
	
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
		_w5500->IP = dhcp->YourIP;
		Stream optData(dhcp->Next(), len);
		PareOption(optData);

		// 向网络宣告已经确认使用哪一个DHCP服务器提供的IP地址
		// 这里其实还应该发送ARP包确认IP是否被占用，如果被占用，还需要拒绝服务器提供的IP，比较复杂，可能性很低，暂时不考虑
#if NET_DEBUG
		debug_printf("DHCP::Offer IP:");
		_w5500->IP.Show();
		debug_printf(" From ");
		remote.Show();
		debug_printf("\r\n");
#endif

		// 独立分配缓冲区进行操作，避免数据被其它线程篡改
		byte buf[sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER) + sizeof(DHCP_HEADER) + 100];
		Stream ms(buf, ArrayLength(buf));
		ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(UDP_HEADER));

		DHCP_HEADER* dhcp2 = ms.Retrieve<DHCP_HEADER>();

		dhcp2->Init(dhcpid, true);
		Request(*dhcp2);
	}
	else if(opt->Data == DHCP_TYPE_Ack)
	{
		_w5500->IP = dhcp->YourIP;
#if NET_DEBUG
		debug_printf("DHCP::Ack   IP:");
		IPAddress(dhcp->YourIP).Show();
		debug_printf(" From ");
		remote.Show();
		debug_printf("\r\n");
#endif

		//if(dhcp->YourIP == _w5500->IP)
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
					//debug_printf("Dhcp过期获取 ");
					Sys.AddTask(RenewDHCP, _w5500, (ulong)time / 2 * 1000000, -1, "DHCP超时");
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
		_w5500->IP.Show();
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
