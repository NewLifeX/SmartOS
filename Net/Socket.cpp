#include "Socket.h"
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
	byte	Mode;	// 无线模式。0不是无线，1是STA，2是AP，3是混合
	byte	Reserved;

	uint	DHCPServer;
	uint	DNSServer;
	uint	DNSServer2;
	uint	Gateway;

	char	SSID[32];
	char	Pass[32];
};

ISocketHost::ISocketHost()
{
	Mode	= SocketMode::Wire;

	SSID	= nullptr;
	Pass	= nullptr;

	//NetReady	= nullptr;
}

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

	Mode	= SocketMode::Wire;

	if(SSID)	SSID->Clear();
	if(Pass)	Pass->Clear();
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
	Mode		= (SocketMode)nc.Mode;

	DHCPServer	= nc.DHCPServer;
	DNSServer	= nc.DNSServer;
	DNSServer2	= nc.DNSServer2;
	Gateway		= nc.Gateway;

	if(SSID) *SSID	= nc.SSID;
	if(Pass) *Pass	= nc.Pass;

	return true;
}

bool ISocketHost::SaveConfig()
{
	if(!Config::Current) return false;

	NetConfig nc;
	nc.IP	= IP.Value;
	nc.Mask	= Mask.Value;
	Mac.CopyTo(nc.Mac);
	nc.Mode	= Mode;

	nc.DHCPServer	= DHCPServer.Value;
	nc.DNSServer	= DNSServer.Value;
	nc.DNSServer2	= DNSServer2.Value;
	nc.Gateway		= Gateway.Value;

	if(SSID) SSID->CopyTo(0, nc.SSID, ArrayLength(nc.SSID) - 1);
	if(Pass) Pass->CopyTo(0, nc.Pass, ArrayLength(nc.Pass) - 1);

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
	net_printf("\r\n    Mode:\t");
	switch(Mode)
	{
		case SocketMode::Wire:
			net_printf("有线");
			break;
		case SocketMode::Station:
			net_printf("无线Station");
			break;
		case SocketMode::AP:
			net_printf("无线AP热点");
			break;
		case SocketMode::STA_AP:
			net_printf("无线Station+AP热点");
			break;
	}
	//net_printf("\r\n");

	if(SSID) { net_printf("\r\n    SSID:\t"); SSID->Show(false); }
	if(Pass) { net_printf("\r\n    Pass:\t"); Pass->Show(false); }

	net_printf("\r\n");
#endif
}

// DNS解析。默认仅支持字符串IP地址解析
IPAddress ISocketHost::QueryDNS(const String& domain)
{
	return IPAddress::Parse(domain);
}

bool ISocketHost::IsStation() const
{
	return Mode == SocketMode::Station || Mode == SocketMode::STA_AP;
}

bool ISocketHost::IsAP() const
{
	return Mode == SocketMode::AP || Mode == SocketMode::STA_AP;
}
