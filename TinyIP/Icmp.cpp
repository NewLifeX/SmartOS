#include "Icmp.h"
#include "Arp.h"

#define NET_DEBUG DEBUG

IcmpSocket::IcmpSocket(TinyIP* tip) : Socket(tip)
{
	Type = IP_ICMP;

	Enable = true;
}

bool IcmpSocket::Process(IP_HEADER& ip, Stream& ms)
{
	ICMP_HEADER* icmp = ms.Retrieve<ICMP_HEADER>();
	if(!icmp) return false;

	IPAddress remote = ip.SrcIP;

	uint len = ms.Remain();
	if(OnPing)
	{
		// 返回值指示是否向对方发送数据包
		bool rs = OnPing(*this, *icmp, icmp->Next(), len);
		if(!rs) return true;
	}
	else
	{
#if NET_DEBUG
		if(icmp->Type != 0)
			debug_printf("Ping From "); // 打印发方的ip
		else
			debug_printf("Ping Reply "); // 打印发方的ip
		remote.Show();
		debug_printf(" Payload=%d ", len);
		// 越过2个字节标识和2字节序列号
		debug_printf("ID=0x%04X Seq=0x%04X ", __REV16(icmp->Identifier), __REV16(icmp->Sequence));
		Sys.ShowString(icmp->Next(), len);
		debug_printf(" \r\n");
#endif
	}

	// 只处理ECHO请求
	if(icmp->Type != 8) return true;

	icmp->Type = 0; // 响应
	// 因为仅仅改变类型，因此我们能够提前修正校验码
	icmp->Checksum += 0x08;

	// 这里不能直接用sizeof(ICMP_HEADER)，而必须用len，因为ICMP包后面一般有附加数据
    Tip->SendIP(IP_ICMP, remote, (byte*)icmp, icmp->Size() + len);

	return true;
}

bool PingCallback(TinyIP* tip, void* param, Stream& ms)
{
	ETH_HEADER* eth = ms.Retrieve<ETH_HEADER>();
	IP_HEADER* _ip = ms.Retrieve<IP_HEADER>();

	if(eth->Type == ETH_IP && _ip->Protocol == IP_ICMP)
	{
		ICMP_HEADER* icmp = (ICMP_HEADER*)_ip->Next();
		int* ps = (int*)param;
		uint   ip  = ps[0];
		ushort id  = ps[1];
		ushort seq = ps[2];

		if(icmp->Identifier == id && icmp->Sequence == seq
		&& _ip->DestIP == tip->IP.Value
		&& _ip->SrcIP == ip)
		{
			return true;
		}
	}

	return false;
}

// Ping目的地址，附带a~z重复的负载数据
bool IcmpSocket::Ping(IPAddress& ip, uint payloadLength)
{
	byte buf[sizeof(ETH_HEADER) + sizeof(IP_HEADER) + sizeof(ICMP_HEADER) + 64];
	uint bufSize = ArrayLength(buf);
	// 注意，此时指针位于0，而内容长度为缓冲区长度
	Stream ms(buf, bufSize);

	ETH_HEADER* eth = (ETH_HEADER*)buf;
	IP_HEADER* _ip = (IP_HEADER*)eth->Next();
	ICMP_HEADER* icmp = (ICMP_HEADER*)_ip->Next();
	icmp->Init(true);

	icmp->Type = 8;
	icmp->Code = 0;

	byte* data = icmp->Next();
	for(int i=0, k=0; i<payloadLength; i++, k++)
	{
		if(k >= 23) k-=23;
		*data++ = ('a' + k);
	}

	//ushort now = Time.Current() / 1000;
	ushort now = Time.Current() >> 10;
	ushort id = __REV16(Sys.ID[0]);
	ushort seq = __REV16(now);
	icmp->Identifier = id;
	icmp->Sequence = seq;

	icmp->Checksum = 0;
	icmp->Checksum = __REV16(Tip->CheckSum(&ip, (byte*)icmp, sizeof(ICMP_HEADER) + payloadLength, 0));

#if NET_DEBUG
	debug_printf("Ping ");
	ip.Show();
	debug_printf(" with Identifier=0x%04x Sequence=0x%04x\r\n", id, seq);
#endif
	Tip->SendIP(IP_ICMP, ip, (byte*)icmp, sizeof(ICMP_HEADER) + payloadLength);

	// 等待时间
	int ps[] = {ip.Value, id, seq};
	if(Tip->LoopWait(PingCallback, ps, 1000)) return true;

	return false;
}
