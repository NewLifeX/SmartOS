#include "Net.h"
#include "Config.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

/******************************** ISocketHost ********************************/

struct NetConfig
{
	uint	IP;		// 本地IP地址
    uint	Mask;	// 子网掩码
	byte	Mac[6];	// 本地Mac地址
	byte	Wireless;	// 无线模式。0不是无线，1是STA，2是AP，3是混合
	byte	Reserved;

	uint	DHCPServer;
	uint	DNSServer;
	uint	DNSServer2;
	uint	Gateway;
};

void ISocketHost::InitConfig()
{
	IPAddress defip(192, 168, 1, 1);

	// 随机IP，取ID最后一个字节
	IP = defip;
	byte first = Sys.ID[0];
	if(first <= 1 || first >= 254) first = 2;
	IP[3] = first;

	Mask = IPAddress(255, 255, 255, 0);
	DHCPServer = Gateway = defip;

	// 修改为双DNS方案，避免单点故障。默认使用阿里和百度公共DNS。
	DNSServer	= IPAddress(223, 5, 5, 5);
	DNSServer2	= IPAddress(180, 76, 76, 76);

	auto& mac = Mac;
	// 随机Mac，前2个字节取自ASCII，最后4个字节取自后三个ID
	//mac[0] = 'W'; mac[1] = 'S';
	// 第一个字节最低位为1表示组播地址，所以第一个字节必须是偶数
	mac[0] = 'N'; mac[1] = 'X';
	for(int i=0; i< 4; i++)
		mac[2 + i] = Sys.ID[3 - i];

	Wireless	= 0;
}

bool ISocketHost::LoadConfig()
{
	if(!Config::Current) return false;

	NetConfig nc;
	Buffer bs(&nc, sizeof(nc));
	if(!Config::Current->Get("NET", bs)) return false;

	IP			= nc.IP;
	Mask		= nc.Mask;
	Mac			= nc.Mac;
	Wireless	= nc.Wireless;

	DHCPServer	= nc.DHCPServer;
	DNSServer	= nc.DNSServer;
	DNSServer2	= nc.DNSServer2;
	Gateway		= nc.Gateway;

	return true;
}

bool ISocketHost::SaveConfig()
{
	if(!Config::Current) return false;

	NetConfig nc;
	nc.IP	= IP.Value;
	nc.Mask	= Mask.Value;
	Mac.CopyTo(nc.Mac);
	nc.Wireless	= Wireless;

	nc.DHCPServer	= DHCPServer.Value;
	nc.DNSServer	= DNSServer.Value;
	nc.DNSServer2	= DNSServer2.Value;
	nc.Gateway		= Gateway.Value;

	Buffer bs(&nc, sizeof(nc));
	return Config::Current->Set("NET", bs);
}

void ISocketHost::ShowConfig()
{
#if NET_DEBUG
	net_printf("    MAC:\t");
	Mac.Show();
	net_printf("\r\n    IP:\t");
	IP.Show();
	net_printf("\r\n    Mask:\t");
	Mask.Show();
	net_printf("\r\n    Gate:\t");
	Gateway.Show();
	net_printf("\r\n    DHCP:\t");
	DHCPServer.Show();
	net_printf("\r\n    DNS:\t");
	DNSServer.Show();
	if(!DNSServer2.IsAny())
	{
		net_printf("\r\n    DNS2:\t");
		DNSServer2.Show();
	}
	net_printf("\r\n    模式:\t");
	if(!Wireless)
		net_printf("有线");
	else
	{
		switch(Wireless)
		{
			case 1:
				net_printf("无线Station");
				break;
			case 2:
				net_printf("无线AP热点");
				break;
			case 3:
				net_printf("无线Station+AP热点");
				break;
		}
		net_printf("\r\n");
	}
	net_printf("\r\n");
#endif
}
