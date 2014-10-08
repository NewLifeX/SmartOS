#include "Arp.h"

#define NET_DEBUG DEBUG

ArpSocket::ArpSocket(TinyIP* tip) : Socket(tip)
{
	Type = ETH_ARP;

#ifdef STM32F0
	Count = 4;
#elif defined(STM32F1)
	Count = 16;
#elif defined(STM32F4)
	Count = 64;
#endif
	_Arps = NULL;
}

ArpSocket::~ArpSocket()
{
	if(_Arps) delete _Arps;
	_Arps = NULL;
}

bool ArpSocket::Process(MemoryStream* ms)
{
	// 如果ms为空，可能是纯为了更新ARP表
	if(!ms)
	{
		Add(Tip->RemoteIP, Tip->RemoteMac);
		return false;
	}

	// 前面的数据长度很不靠谱，这里进行小范围修正
	//uint size = ms->Position + sizeof(ARP_HEADER);
	//if(ms->Length < size) ms->Length = size;

	ARP_HEADER* arp = ms->Retrieve<ARP_HEADER>();
	if(!arp) return false;

	/*
	当封装的ARP报文在以太网上传输时，硬件类型字段赋值为0x0100，标识硬件为以太网硬件;
	协议类型字段赋值为0x0800，标识上次协议为IP协议;
	由于以太网的MAC地址为48比特位，IP地址为32比特位，则硬件地址长度字段赋值为6，协议地址长度字段赋值为4;
	选项字段标识ARP报文的类型，当为请求报文时，赋值为0x0100，当为回答报文时，赋值为0x0200。
	*/

	// 如果是Arp响应包，自动加入缓存
	/*if(arp->Option == 0x0200)
	{
		AddArp(arp->SrcIP, arp->SrcMac);
	}
	// 别人的响应包这里收不到呀，还是把请求包也算上吧
	if(arp->Option == 0x0100)
	{
		AddArp(arp->SrcIP, arp->SrcMac);
	}*/

	// 是否发给本机。
	if(arp->DestIP != Tip->IP) return true;

#if NET_DEBUG
	// 数据校验
	assert_param(arp->HardType == 0x0100);
	assert_param(arp->ProtocolType == ETH_IP);
	assert_param(arp->HardLength == 6);
	assert_param(arp->ProtocolLength == 4);
	assert_param(arp->Option == 0x0100);

	if(arp->Option == 0x0100)
		debug_printf("ARP Request For ");
	else
		debug_printf("ARP Response For ");

	Tip->ShowIP(arp->DestIP);
	debug_printf(" <= ");
	Tip->ShowIP(arp->SrcIP);
	debug_printf(" [");
	Tip->ShowMac(arp->SrcMac);
	debug_printf("] Payload=%d\r\n", ms->Remain());
#endif

	// 构造响应包
	arp->Option = 0x0200;
	// 来源IP和Mac作为目的地址
	arp->DestMac = arp->SrcMac;
	arp->SrcMac  = Tip->Mac;
	arp->DestIP  = arp->SrcIP;
	arp->SrcIP   = Tip->IP;

#if NET_DEBUG
	debug_printf("ARP Response To ");
	Tip->ShowIP(arp->DestIP);
	debug_printf(" size=%d\r\n", sizeof(ARP_HEADER));
#endif

	Tip->SendEthernet(ETH_ARP, (byte*)arp, sizeof(ARP_HEADER));

	return true;
}

// 请求Arp并返回其Mac。timeout超时3秒，如果没有超时时间，表示异步请求，不用等待结果
const MacAddress* ArpSocket::Request(IPAddress ip, int timeout)
{
	// 缓冲区必须略大，否则接收数据时可能少一个字节
	byte buf[sizeof(ETH_HEADER) + sizeof(ARP_HEADER) + 4];
	uint bufSize = ArrayLength(buf);
	MemoryStream ms(buf, bufSize);

	ETH_HEADER* eth = (ETH_HEADER*)buf;
	ARP_HEADER* arp = (ARP_HEADER*)eth->Next();
	arp->Init();

	// 构造请求包
	arp->Option = 0x0100;
	Tip->RemoteMac = 0;
	arp->DestMac = 0;
	arp->SrcMac  = Tip->Mac;
	arp->DestIP  = ip;
	arp->SrcIP   = Tip->IP;

#if NET_DEBUG
	debug_printf("ARP Request To ");
	Tip->ShowIP(arp->DestIP);
	debug_printf(" size=%d\r\n", sizeof(ARP_HEADER));
#endif

	Tip->SendEthernet(ETH_ARP, (byte*)arp, sizeof(ARP_HEADER));

	// 如果没有超时时间，表示异步请求，不用等待结果
	if(timeout <= 0) return NULL;

	// 总等待时间
	TimeWheel tw(1, 0, 0);
	while(!tw.Expired())
	{
		// 阻塞其它任务，频繁调度OnWork，等待目标数据
		uint len = Tip->Fetch(buf, bufSize);
		if(!len)
		{
			Sys.Sleep(2);	// 等待一段时间，释放CPU

			continue;
		}

		// 处理ARP
		if(eth->Type == ETH_ARP)
		{
			// 是否目标发给本机的Arp响应包。注意memcmp相等返回0
			if(arp->DestIP == Tip->IP
			// 不要那么严格，只要有源MAC地址，即使不是发给本机，也可以使用
			&& arp->Option == 0x0200)
			{
				return &arp->SrcMac;
			}
		}

		// 用不到数据包交由系统处理
		ms.SetPosition(0);
		ms.Length = len;
		Tip->Process(&ms);
	}

	return NULL;
}

const MacAddress* ArpSocket::Resolve(IPAddress ip)
{
	if(Tip->IsBroadcast(ip)) return NULL;

	// 如果不在本子网，那么应该找网关的Mac
	if((ip & Tip->Mask) != (Tip->IP & Tip->Mask)) ip = Tip->Gateway;

	ARP_ITEM* item = NULL;	// 匹配项
	if(_Arps)
	{
		uint sNow = Time.Current() >> 20;	// 当前时间，秒
		// 在表中查找
		for(int i=0; i<Count; i++)
		{
			ARP_ITEM* arp = &_Arps[i];
			if(arp->IP == ip)
			{
				// 如果未过期，则直接使用。否则重新请求
				if(arp->Time > sNow) return &arp->Mac;

				// 暂时保存，待会可能请求失败，还可以用旧的顶着
				item = arp;
			}
		}
	}

	// 找不到则发送Arp请求。如果有旧值，则使用异步请求即可
	const MacAddress* mac = Request(ip, item ? 0 : 3);
	if(!mac) return item ? &item->Mac : NULL;

	Add(ip, *mac);

	return mac;
}

void ArpSocket::Add(IPAddress ip, const MacAddress& mac)
{
	if(!ip || ip == 0xFFFFFFFF) return;

#if NET_DEBUG
	debug_printf("Add Arp(");
	TinyIP::ShowIP(ip);
	debug_printf(", ");
	TinyIP::ShowMac(mac);
	debug_printf(")\r\n");
#endif

	if(!_Arps)
	{
		_Arps = new ARP_ITEM[Count];
		memset(_Arps, 0, sizeof(ARP_ITEM) * Count);
	}

	ARP_ITEM* item = NULL;
	ARP_ITEM* empty = NULL;
	// 在表中查找项
	for(int i=0; i<Count; i++)
	{
		ARP_ITEM* arp = &_Arps[i];
		if(arp->IP == ip)
		{
			item = arp;
			break;
		}
		if(!empty && arp->IP == 0)
		{
			empty = arp;
			break;
		}
	}

	// 如果没有匹配项，则使用空项
	if(!item) item = empty;
	// 如果也没有空项，表示满了，那就替换最老的一个
	if(!item)
	{
		uint oldTime = 0xFFFFFFFF;
		// 在表中查找最老项用于替换
		for(int i=0; i<Count; i++)
		{
			ARP_ITEM* arp = &_Arps[i];
			// 找最老的一个，待会如果需要覆盖，就先覆盖它。避开网关
			if(arp->Time < oldTime && arp->IP != Tip->Gateway)
			{
				oldTime = arp->Time;
				item = arp;
			}
		}
#if NET_DEBUG
		debug_printf("Arp Table is full, replace ");
		TinyIP::ShowIP(item->IP);
		debug_printf("\r\n");
#endif
	}

	//uint sNow = Time.Current() / 1000000;	// 当前时间，秒
	uint sNow = Time.Current() >> 20;	// 当前时间，秒
	// 保存
	item->IP = ip;
	item->Mac = mac;
	item->Time = sNow + 60;	// 默认过期时间1分钟
}
