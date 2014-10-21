#include "TinyIP.h"

#define NET_DEBUG DEBUG

TinyIP::TinyIP(ITransport* port)
{
	_port = port;
	_StartTime = Time.Current();

	const byte defip_[] = {192, 168, 0, 1};
	IPAddress defip = *(uint*)defip_;

	// 随机IP，取ID最后一个字节
	IP = defip & 0x00FFFFFF;
	byte first = Sys.ID[0];
	if(first <= 1 || first >= 254) first = 2;
	IP |= first << 24;

	// 随机Mac，前三个字节取自YWS的ASCII，最后3个字节取自后三个ID
	byte* mac = (byte*)&Mac;
	mac[0] = 'P'; mac[1] = 'M'; //mac[2] = 'X';
	//memcpy(Mac, "YWS", 3);
	memcpy(&mac[2], (byte*)Sys.ID, 6 - 2);
	// MAC地址首字节奇数表示组地址，这里用偶数
	//Mac[0] = 'N'; Mac[1] = 'X';
	//memcpy(&Mac[2], (byte*)Sys.ID, 6 - 2);

	Mask = 0x00FFFFFF;
	DHCPServer = Gateway = DNSServer = defip;

	Buffer = NULL;
	BufferSize = 1500;
	EnableBroadcast = true;
	EnableArp = true;

	Sockets.SetCapacity(0x10);
	// 必须有Arp，否则无法响应别人的IP询问
	//Arp = new ArpSocket(this);
	Arp = NULL;
}

TinyIP::~TinyIP()
{
	if(_port) delete _port;
    _port = NULL;

	if(Buffer) delete Buffer;
	Buffer = NULL;

	if(Arp) delete Arp;
	Arp = NULL;
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
	if(eth->DestMac != Mac && !eth->DestMac.IsBroadcast()) return 0;

	return len;
}

void TinyIP::Process(MemoryStream* ms)
{
	if(!ms) return;

	ETH_HEADER* eth = ms->Retrieve<ETH_HEADER>();
	if(!eth) return;

	// 只处理发给本机MAC的数据包。此时不能进行目标Mac地址过滤，因为可能是广播包
	//if(eth->DestMac != Mac) return;
	LocalMac  = eth->DestMac;
	RemoteMac = eth->SrcMac;

	// 处理ARP
	if(eth->Type == ETH_ARP)
	{
		if(EnableArp && Arp) Arp->Process(ms);

		return;
	}

	IP_HEADER* ip = ms->Retrieve<IP_HEADER>();
	// 是否发给本机。注意memcmp相等返回0
	if(!ip || !IsMyIP(ip->DestIP)) return;

#if NET_DEBUG
	if(eth->Type != ETH_IP)
	{
		debug_printf("Unkown EthernetType 0x%02X From", eth->Type);
		ShowIP(ip->SrcIP);
		debug_printf("\r\n");
	}
#endif

	// 记录远程信息
	LocalIP = ip->DestIP;
	RemoteIP = ip->SrcIP;

	// 移交给ARP处理，为了让它更新ARP表
	if(Arp) Arp->Process(NULL);

	//!!! 太杯具了，收到的数据包可能有多余数据，这两个长度可能不等
	//assert_param(__REV16(ip->TotalLength) == len);
	// 数据包是否完整
	//if(ms->Remain() < __REV16(ip->TotalLength)) return;
	// 计算负载数据的长度，注意ip可能变长，长度Length的单位是4字节
	//len -= sizeof(IP_HEADER);

	// 前面的len不准确，必须以这个为准
	uint size = __REV16(ip->TotalLength) - (ip->Length << 2);
	ms->Length = ms->Position() + size;
	//len = size;
	//buf += (ip->Length << 2);
	ms->Seek((ip->Length << 2) - sizeof(IP_HEADER));

	// 各处理器有可能改变数据流游标，这里备份一下
	uint p = ms->Position();
	//for(int i=0; i < Sockets.Count(); i++)
	// 考虑到可能有通用端口处理器，也可能有专用端口处理器（一般在后面），这里偷懒使用倒序处理
	uint count = Sockets.Count();
	for(int i=count-1; i>=0; i--)
	{
		Socket* socket = Sockets[i];
		if(socket)
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
	debug_printf("IP Unkown Protocol=%d ", ip->Protocol);
	ShowIP(ip->SrcIP);
	debug_printf(" => ");
	ShowIP(ip->DestIP);
	debug_printf("\r\n");
#endif
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
			MemoryStream ms(tip->Buffer, tip->BufferSize);
			ms.Length = len;
			tip->Process(&ms);
		}
	}
}

bool TinyIP::Open()
{
	if(_port->Open()) return true;

	debug_printf("TinyIP Init Failed!\r\n");
	return false;
}

bool TinyIP::Init()
{
	debug_printf("\r\nTinyIP Init...\r\n");

	// 分配缓冲区。比较大，小心堆空间不够
	if(!Buffer)
	{
		Buffer = new byte[BufferSize];
		assert_param(Buffer);
		assert_param(Sys.CheckMemory());
	}

	if(!Open()) return false;

	ShowInfo();

	// 添加到系统任务，马上开始，尽可能多被调度
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
	debug_printf("    IP:\t");
	ShowIP(IP);
	debug_printf("\r\n    Mask:\t");
	ShowIP(Mask);
	debug_printf("\r\n    Gate:\t");
	ShowIP(Gateway);
	debug_printf("\r\n    DHCP:\t");
	ShowIP(DHCPServer);
	debug_printf("\r\n    DNS:\t");
	ShowIP(DNSServer);
	debug_printf("\r\n");
#endif
}

void TinyIP::SendEthernet(ETH_TYPE type, byte* buf, uint len)
{
	ETH_HEADER* eth = (ETH_HEADER*)(buf - sizeof(ETH_HEADER));
	assert_param(IS_ETH_TYPE(type));

	eth->Type = type;
	eth->DestMac = RemoteMac;
	eth->SrcMac  = Mac;

	len += sizeof(ETH_HEADER);
	//if(len < 60) len = 60;	// 以太网最小包60字节

	/*debug_printf("SendEthernet: type=0x%04x, len=%d\r\n", type, len);
	Sys.ShowHex((byte*)eth, len, '-');
	debug_printf("\r\n");*/

	//assert_param((byte*)eth == Buffer);
	_port->Write((byte*)eth, len);
}

void TinyIP::SendIP(IP_TYPE type, byte* buf, uint len)
{
	IP_HEADER* ip = (IP_HEADER*)(buf - sizeof(IP_HEADER));
	assert_param(ip);
	assert_param(IS_IP_TYPE(type));

	ip->DestIP = RemoteIP;
	ip->SrcIP = IP;

	ip->Version = 4;
	//ip->TypeOfService = 0;
	ip->Length = sizeof(IP_HEADER) / 4;	// 暂时不考虑附加数据
	ip->TotalLength = __REV16(sizeof(IP_HEADER) + len);
	ip->Flags = 0x40;
	ip->FragmentOffset = 0;
	ip->TTL = 64;
	ip->Protocol = type;

	// 网络序是大端
	ip->Checksum = 0;
	ip->Checksum = __REV16((ushort)TinyIP::CheckSum((byte*)ip, sizeof(IP_HEADER), 0));

	SendEthernet(ETH_IP, (byte*)ip, sizeof(IP_HEADER) + len);
}

#define TinyIP_HELP
#ifdef TinyIP_HELP
void TinyIP::ShowIP(IPAddress ip)
{
	byte* ips = (byte*)&ip;
	debug_printf("%d", *ips++);
	for(int i=1; i<4; i++)
		debug_printf(".%d", *ips++);
}

void TinyIP::ShowMac(const MacAddress& mac)
{
	byte* m = (byte*)&mac;
	debug_printf("%02X", *m++);
	for(int i=1; i<6; i++)
		debug_printf("-%02X", *m++);
}

uint TinyIP::CheckSum(byte* buf, uint len, byte type)
{
    // type 0=ip
    //      1=udp
    //      2=tcp
    unsigned long sum = 0;

    if(type == 1)
    {
        sum += IP_UDP; // protocol udp
        // the length here is the length of udp (data+header len)
        // =length given to this function - (IP.scr+IP.dst length)
        sum += len - 8; // = real tcp len
    }
    if(type == 2)
    {
        sum += IP_TCP;
        // the length here is the length of tcp (data+header len)
        // =length given to this function - (IP.scr+IP.dst length)
        sum += len - 8; // = real tcp len
    }
    // build the sum of 16bit words
    while(len > 1)
    {
        sum += 0xFFFF & (*buf << 8 | *(buf + 1));
        buf += 2;
        len -= 2;
    }
    // if there is a byte left then add it (padded with zero)
    if (len)
    {
        sum += (0xFF & *buf) << 8;
    }
    // now calculate the sum over the bytes in the sum
    // until the result is only 16bit long
    while (sum>>16)
    {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    // build 1's complement:
    return( (uint) sum ^ 0xFFFF);
}

bool TinyIP::IsMyIP(IPAddress ip)
{
	if(ip == IP) return true;

	if(EnableBroadcast && IsBroadcast(ip)) return true;

	return false;
}

bool TinyIP::IsBroadcast(IPAddress ip)
{
	// 全网广播
	if(ip == 0xFFFFFFFF) return true;

	if(ip == (IP | ~Mask)) return true;

	return false;
}
#endif

Socket::Socket(TinyIP* tip)
{
	assert_param(tip);

	Tip = tip;
	// 除了ARP以外，加入到列表
	if(this->Type != ETH_ARP) tip->Sockets.Add(this);
}

Socket::~Socket()
{
	assert_param(Tip);

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
