#include "Icmp.h"
#include "Arp.h"

#define NET_DEBUG DEBUG

class PingSession
{
public:
	IPAddress	Address;
	ushort		Identifier;
	ushort		Sequence;
	bool		Success;

	PingSession(IPAddress& ip, ushort id, ushort seq)
	{
		Address		= ip;
		Identifier	= id;
		Sequence	= seq;
		Success		= false;
	}
};

// 用于等待Ping响应的会话
PingSession* Session = NULL;

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

	// 检查有没有会话等待
	if(icmp->Type == 0 && Session != NULL && remote == Session->Address)
	{
		Session->Success = true;
		return true;
	}

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
	// 注意，此时指针位于0，而内容长度为缓冲区长度
	Stream ms(buf, ArrayLength(buf));
	ms.Seek(sizeof(ETH_HEADER) + sizeof(IP_HEADER));

	ICMP_HEADER* icmp = ms.Retrieve<ICMP_HEADER>();
	icmp->Init(true);

	icmp->Type = 8;
	icmp->Code = 0;

	// 限定最大长度64
	if(payloadLength > 64) payloadLength = 64;
	for(int i=0, k=0; i<payloadLength; i++, k++)
	{
		if(k >= 23) k-=23;
		ms.Write((byte)('a' + k));
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
	debug_printf("ICMP::Ping ");
	ip.Show();
	debug_printf(" with Identifier=0x%04x Sequence=0x%04x", id, seq);

	ulong start = Time.Current();
#endif
	Tip->SendIP(IP_ICMP, ip, (byte*)icmp, sizeof(ICMP_HEADER) + payloadLength);

	// 等待时间
	//int ps[] = {ip.Value, id, seq};
	//if(Tip->LoopWait(PingCallback, ps, 1000)) return true;

	PingSession ps(ip, id, seq);
	Session = &ps;

	// 等待响应
	TimeWheel tw(0, 1000);
	tw.Sleep = 1;
	do{
		if(ps.Success) break;
	}while(!tw.Expired());

	Session = NULL;

#if NET_DEBUG
	uint cost = (int)(Time.Current() - start) / 1000;
	if(ps.Success)
		debug_printf(" 成功 %dms\r\n", cost);
	else
		debug_printf(" 失败 %dms\r\n", cost);
#endif

	return false;
}
