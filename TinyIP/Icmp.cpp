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

	bool Check(IPAddress& remote, ICMP_HEADER* icmp)
	{
		if(remote != Address) return false;
		if(Identifier != icmp->Identifier) return false;
		if(Sequence != icmp->Sequence) return false;

		return true;
	}
};

// 用于等待Ping响应的会话
PingSession* _IcmpSession = NULL;

IcmpSocket::IcmpSocket(TinyIP* tip) : Socket(tip, IP_ICMP)
{
	//Type = IP_ICMP;

	Enable = true;
}

bool IcmpSocket::Process(IP_HEADER& ip, Stream& ms)
{
	ICMP_HEADER* icmp = ms.Retrieve<ICMP_HEADER>();
	if(!icmp) return false;

	IPAddress remote = ip.SrcIP;

	// 检查有没有会话等待
	if(icmp->Type == 0 && _IcmpSession != NULL && _IcmpSession->Check(remote, icmp))
	{
		_IcmpSession->Success = true;
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
		/*if(icmp->Type != 0)
			debug_printf("Ping From "); // 打印发方的ip
		else
			debug_printf("Ping Reply "); // 打印发方的ip*/
		switch(icmp->Type)
		{
			case 0:
				debug_printf("Ping Reply ");
				break;
			case 3:
				debug_printf("ICMP::应用端口不可达 ");
				break;
			case 8:
				debug_printf("Ping From ");
				break;
			default:
				debug_printf("ICMP%d ", icmp->Type);
				break;
		}
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

// Ping目的地址，附带a~z重复的负载数据
bool IcmpSocket::Ping(IPAddress& ip, uint payloadLength)
{
	assert_param2(this, "非法调用Icmp");
	assert_ptr(this);

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

	TimeCost ct;
#endif
	Tip->SendIP(IP_ICMP, ip, (byte*)icmp, sizeof(ICMP_HEADER) + payloadLength);

	// 等待时间
	//int ps[] = {ip.Value, id, seq};
	//if(Tip->LoopWait(PingCallback, ps, 1000)) return true;

	PingSession ps(ip, id, seq);
	_IcmpSession = &ps;

	// 等待响应
	TimeWheel tw(0, 1000);
	tw.Sleep = 1;
	do{
		if(ps.Success) break;
	}while(!tw.Expired());

	_IcmpSession = NULL;

#if NET_DEBUG
	uint cost = ct.Elapsed() / 1000;
	if(ps.Success)
		debug_printf(" 成功 %dms\r\n", cost);
	else
		debug_printf(" 失败 %dms\r\n", cost);
#endif

	return false;
}
