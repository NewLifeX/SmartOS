#include "TinyIP.h"
#include "Arp.h"

#define NET_DEBUG DEBUG

TinyIP::TinyIP() { Init(); }

TinyIP::TinyIP(ITransport* port)
{
	Init();
	Init(port);
}

TinyIP::~TinyIP()
{
	delete _port;
    _port = NULL;

	delete Buffer;
	Buffer = NULL;

	delete Arp;
	Arp = NULL;
}

void TinyIP::Init()
{
	_port = NULL;
	_StartTime = 0;

	Mask = 0x00FFFFFF;
	DHCPServer = Gateway = DNSServer = IP = 0;

	Buffer = NULL;
	BufferSize = 1500;
	EnableBroadcast = true;
	//EnableArp = true;

	Sockets.SetCapacity(0x10);
	// 必须有Arp，否则无法响应别人的IP询问
	//Arp = new ArpSocket(this);
	Arp = NULL;
}

void TinyIP::Init(ITransport* port)
{
	_port = port;
	_StartTime = Time.Current();

	const byte defip_[] = {192, 168, 0, 1};
	IPAddress defip(defip_);

	// 随机IP，取ID最后一个字节
	IP = defip;
	byte first = Sys.ID[0];
	if(first <= 1 || first >= 254) first = 2;
	//IP.Value &= ((first << 24) | 0x00FFFFFF);
	IP[3] = first;

	// 随机Mac，前三个字节取自YWS的ASCII，最后3个字节取自后三个ID
	byte* mac = (byte*)&Mac;
	mac[0] = 'W'; mac[1] = 'S'; mac[2] = 'W'; mac[3] = 'L';
	//memcpy(Mac, "YWS", 3);
	memcpy(&mac[4], (byte*)Sys.ID, 6 - 4);
	// MAC地址首字节奇数表示组地址，这里用偶数
	//Mac[0] = 'N'; Mac[1] = 'X';
	//memcpy(&Mac[2], (byte*)Sys.ID, 6 - 2);

	Mask = 0x00FFFFFF;
	DHCPServer = Gateway = DNSServer = defip;
}

// 循环调度的任务
uint TinyIP::Fetch(byte* buf, uint len)
{
	if(!buf) buf = Buffer;
	if(!len) len = BufferSize;

	// 获取缓冲区的包
	len = _port->Read(buf, len);
	// 如果缓冲器里面没有数据则转入下一次循环
	if(len < sizeof(ETH_HEADER)/* || !_net->Unpack(len)*/) return 0;

	ETH_HEADER* eth = (ETH_HEADER*)buf;
	// 只处理发给本机MAC的数据包。此时进行目标Mac地址过滤，可能是广播包
	MacAddress dest = eth->DestMac.Value();
	if(dest != Mac && !dest.IsBroadcast()) return 0;

	return len;
}

void TinyIP::Process(Stream* ms)
{
	if(!ms) return;

	ETH_HEADER* eth = ms->Retrieve<ETH_HEADER>();
	if(!eth) return;

	// 只处理发给本机MAC的数据包。此时不能进行目标Mac地址过滤，因为可能是广播包
	//if(eth->DestMac != Mac) return;
	//LocalMac  = eth->DestMac;
	RemoteMac = eth->SrcMac.Value();

	// 处理ARP
	if(eth->Type == ETH_ARP)
	{
		//if(EnableArp && Arp) Arp->Process(ms);
		if(Arp && Arp->Enable) Arp->Process(ms);

		return;
	}

	IP_HEADER* ip = ms->Retrieve<IP_HEADER>();
	// 是否发给本机。
	if(!ip) return;
	IPAddress addr = ip->DestIP;
	if(!IsMyIP(addr)) return;

	/*Sys.ShowHex((byte*)ip, ip->Size(), '-');
	debug_printf("\r\n");
	Sys.ShowHex((byte*)ip->Next(), __REV16(ip->TotalLength) - ip->Size(), '-');
	debug_printf("\r\n");*/

#if NET_DEBUG
	if(eth->Type != ETH_IP)
	{
		debug_printf("Unkown EthernetType 0x%02X From ", eth->Type);
		IPAddress(ip->SrcIP).Show();
		debug_printf("\r\n");
	}
#endif

	// 记录远程信息
	//LocalIP = ip->DestIP;
	RemoteIP = ip->SrcIP;

	// 移交给ARP处理，为了让它更新ARP表
	//if(Arp && Arp->Enable) Arp->Process(NULL);
	if(Arp) Arp->Process(NULL);

	FixPayloadLength(ip, ms);

	// 各处理器有可能改变数据流游标，这里备份一下
	uint p = ms->Position();
	//for(int i=0; i < Sockets.Count(); i++)
	// 考虑到可能有通用端口处理器，也可能有专用端口处理器（一般在后面），这里偷懒使用倒序处理
	uint count = Sockets.Count();
	for(int i=count-1; i>=0; i--)
	{
		Socket* socket = Sockets[i];
		if(socket && socket->Enable)
		{
			// 必须类型匹配
			if(socket->Type == ip->Protocol)
			{
				// 如果处理成功，则中断遍历
				if(socket->Process(ms)) return;
				ms->SetPosition(p);
			}
		}
	}

#if NET_DEBUG
	/*debug_printf("IP Unkown Protocol=%d ", ip->Protocol);
	ShowIP(ip->SrcIP);
	debug_printf(" => ");
	ShowIP(ip->DestIP);
	debug_printf("\r\n");*/
#endif
}

// 修正IP包负载数据的长度。物理层送来的长度可能有误，一般超长
void TinyIP::FixPayloadLength(IP_HEADER* ip, Stream* ms)
{
	// 前面的len不准确，必须以这个为准
	uint size = __REV16(ip->TotalLength) - (ip->Length << 2);
	ms->Length = ms->Position() + size;
	//len = size;
	//buf += (ip->Length << 2);
	ms->Seek((ip->Length << 2) - sizeof(IP_HEADER));
}

// 任务函数
void TinyIP::Work(void* param)
{
	TinyIP* tip = (TinyIP*)param;
	if(tip)
	{
		uint len = tip->Fetch();
		if(len)
		{
			// 注意，此时指针位于0，而内容长度为缓冲区长度
			Stream ms(tip->Buffer, tip->BufferSize);
			ms.Length = len;
			tip->Process(&ms);
		}
	}
}

bool TinyIP::LoopWait(LoopFilter filter, void* param, uint msTimeout)
{
	Stream ms(Buffer, BufferSize);

	// 总等待时间
	TimeWheel tw(0, msTimeout, 0);
	while(!tw.Expired())
	{
		// 阻塞其它任务，频繁调度OnWork，等待目标数据
		uint len = Fetch();
		if(!len)
		{
			Sys.Sleep(2);	// 等待一段时间，释放CPU

			continue;
		}

		// 业务
		ms.SetPosition(0);
		ms.Length = len;
		if(filter(this, param, ms)) return true;

		// 用不到数据包交由系统处理
		ms.SetPosition(0);
		ms.Length = len;
		Process(&ms);
	}

	return false;
}

bool TinyIP::Open()
{
	debug_printf("\r\nTinyIP::Open...\r\n");

	if(!_port->Open())
	{
		debug_printf("TinyIP::Open Failed!\r\n");
		return false;
	}

	// 分配缓冲区。比较大，小心堆空间不够
	if(!Buffer)
	{
		Buffer = new byte[BufferSize];
		assert_param(Buffer);
		assert_param(Sys.CheckMemory());
	}

	// 必须有Arp，否则无法响应别人的IP询问
	if(!Arp) Arp = new ArpSocket(this);
	Arp->Enable = true;

	//if(!Open()) return false;

	ShowInfo();

	// 添加到系统任务，马上开始，尽可能多被调度
	debug_printf("以太网轮询 ");
    Sys.AddTask(Work, this);

#if NET_DEBUG
	uint us = Time.Current() - _StartTime;
	debug_printf("TinyIP Ready! Cost:%dms\r\n\r\n", us / 1000);
#endif

	return true;
}

void TinyIP::ShowInfo()
{
#if NET_DEBUG
	String str;
	
	debug_printf("    IP:\t");
	IP.Show();
	debug_printf("\r\n    Mask:\t");
	Mask.Show();
	debug_printf("\r\n    Gate:\t");
	Gateway.Show();
	debug_printf("\r\n    DHCP:\t");
	DHCPServer.Show();
	debug_printf("\r\n    DNS:\t");
	DNSServer.Show();
	debug_printf("\r\n");
#endif
}

void TinyIP::SendEthernet(ETH_TYPE type, const byte* buf, uint len)
{
	ETH_HEADER* eth = (ETH_HEADER*)(buf - sizeof(ETH_HEADER));
	assert_param(IS_ETH_TYPE(type));

	eth->Type = type;
	eth->DestMac = RemoteMac;
	eth->SrcMac  = Mac;

	len += sizeof(ETH_HEADER);
	//if(len < 60) len = 60;	// 以太网最小包60字节

	/*string name = "Unkown";
	switch(type)
	{
		case ETH_ARP: { name = "ARP"; break; }
		case ETH_IP: { name = "IP"; break; }
		case ETH_IPv6: { name = "IPv6"; break; }
	}
	debug_printf("SendEthernet: type=0x%04x %s, len=%d(0x%x) ", type, name, len, len);
	ShowMac(Mac);
	debug_printf(" => ");
	ShowMac(RemoteMac);
	debug_printf("\r\n");*/
	/*Sys.ShowHex((byte*)eth->Next(), len, '-');
	debug_printf("\r\n");*/

	//assert_param((byte*)eth == Buffer);
	_port->Write((byte*)eth, len);
}

void TinyIP::SendIP(IP_TYPE type, const byte* buf, uint len)
{
	IP_HEADER* ip = (IP_HEADER*)(buf - sizeof(IP_HEADER));
	assert_param(ip);
	assert_param(IS_IP_TYPE(type));

	ip->DestIP = RemoteIP.Value;
	ip->SrcIP = IP.Value;

	ip->Version = 4;
	//ip->TypeOfService = 0;
	ip->Length = sizeof(IP_HEADER) / 4;	// 暂时不考虑附加数据
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + len);
	//ip->Flags = 0x40;
	//ip->FragmentOffset = 0;
	//ip->TTL = 64;
	ip->Protocol = type;

	// 报文唯一标识。用于识别重组等
	static ushort g_Identifier = 1;
	ip->Identifier = __REV16(g_Identifier++);

	// 网络序是大端
	ip->Checksum = 0;
	ip->Checksum = __REV16(CheckSum((byte*)ip, sizeof(IP_HEADER), 0));

	assert_ptr(Arp);
	ArpSocket* arp = (ArpSocket*)Arp;
	MacAddress mac;
	if(!arp->Resolve(RemoteIP, mac))
	{
#if NET_DEBUG
		debug_printf("No Mac For ");
		RemoteIP.Show();
		debug_printf("\r\n");
#endif
		return;
	}
	RemoteMac = mac;

	/*string name = "Unkown";
	switch(type)
	{
		case IP_ICMP: { name = "ICMP"; break; }
		case IP_IGMP: { name = "IGMP"; break; }
		case IP_TCP: { name = "TCP"; break; }
		case IP_UDP: { name = "UDP"; break; }
	}
	debug_printf("SendIP: type=0x%04x %s, len=%d(0x%x) ", type, name, len, len);
	ShowIP(IP);
	debug_printf(" => ");
	ShowIP(RemoteIP);
	debug_printf("\r\n");*/

	SendEthernet(ETH_IP, (byte*)ip, sizeof(IP_HEADER) + len);
}

#define TinyIP_HELP
#ifdef TinyIP_HELP
ushort TinyIP::CheckSum(const byte* buf, uint len, byte type)
{
    // type 0=ip
    //      1=udp
    //      2=tcp
    uint sum = 0;

	// !!谨记网络是大端
    if(type == 1 || type == 2)
	{
        // UDP/TCP的校验和需要计算UDP首部加数据荷载部分，但也需要加上UDP伪首部。
		// 这个伪首部指，源地址、目的地址、UDP数据长度、协议类型（0x11），协议类型就一个字节，但需要补一个字节的0x0，构成12个字节。
		// 源地址。其实可以按照4字节累加，反正后面会把高位移位到低位累加，但是得考虑溢出的问题。
		sum += __REV16(IP.Value & 0xFFFF);
		sum += __REV16(IP.Value >> 16);
		sum += __REV16(RemoteIP.Value & 0xFFFF);
		sum += __REV16(RemoteIP.Value >> 16);

		// 数据长度
		sum += len;

		// 加上协议类型
		if(type == 1)
			sum += IP_UDP;
		else if(type == 2)
			sum += IP_TCP;
	}
    // 按16位字计算和
    while(len > 1)
    {
        sum += 0xFFFF & (*buf << 8 | *(buf + 1));
        buf += 2;
        len -= 2;
    }
    // 如果字节个数不是偶数个，这里会剩余1，后面补0
    if (len)
    {
        sum += (0xFF & *buf) << 8;
    }
    // 现在计算sum字节的和，直到只有16位长
    while (sum >> 16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    // 取补码
    return (ushort)(sum ^ 0xFFFF);
}

bool TinyIP::IsMyIP(IPAddress& ip)
{
	if(ip == IP) return true;

	if(EnableBroadcast && IsBroadcast(ip)) return true;

	return false;
}

bool TinyIP::IsBroadcast(IPAddress& ip)
{
	// 全网广播
	if(ip.IsBroadcast()) return true;

	if(ip.Value == (IP.Value | ~Mask.Value)) return true;

	return false;
}
#endif

Socket::Socket(TinyIP* tip)
{
	assert_param(tip);

	Tip = tip;
	Enable = false;
	// 除了ARP以外，加入到列表
	if(this->Type != ETH_ARP) tip->Sockets.Add(this);
}

Socket::~Socket()
{
	assert_param(Tip);

	Enable = false;
	// 从TinyIP中删除当前Socket
	Tip->Sockets.Remove(this);
}

Socket* SocketList::FindByType(ushort type)
{
	uint count = Count();
	for(int i=count-1; i>=0; i--)
	{
		Socket* socket = (*this)[i];
		if(socket)
		{
			// 必须类型匹配
			if(socket->Type == type) return socket;
		}
	}

	return NULL;
}
