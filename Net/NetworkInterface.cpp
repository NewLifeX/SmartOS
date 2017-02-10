#include "NetworkInterface.h"
#include "Config.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

List<NetworkInterface*>	NetworkInterface::All;

/******************************** NetworkInterface ********************************/

struct NetConfig
{
	uint	IP;		// 本地IP地址
    uint	Mask;	// 子网掩码
	byte	Mac[6];	// 本地Mac地址
	byte	Mode;	// 无线模式。0不是无线，1是STA，2是AP，3是混合
	byte	Reserved;

	uint	DNSServer;
	uint	DNSServer2;
	uint	Gateway;

	/*char	SSID[32];
	char	Pass[32];*/
	char	Data[4 * (32 - 7)];	// 扩展数据，给各接口保存自定义数据
};

NetworkInterface::NetworkInterface()
{
	Name	= "Network";
	Speed	= 0;
	Opened	= false;
	Linked	= false;

	Mode	= NetworkType::Wire;
	Status	= NetworkStatus::None;

	_taskLink	= 0;
	_retry	= 0;

	debug_printf("Network::Add 0x%p\r\n", this);
	All.Add(this);
}

NetworkInterface::~NetworkInterface()
{
	debug_printf("Network::Remove 0x%p\r\n", this);
	All.Remove(this);

	if(!Opened) Close();
}

bool NetworkInterface::Open()
{
	if(Opened) return true;

	// 打开接口
	Opened	= OnOpen();

	// 建立连接任务
	_taskLink	= Sys.AddTask([](void* s){ ((NetworkInterface*)s)->OnLoop(); }, this, 0, 1000, "网络检测");

	return Opened;
}

void NetworkInterface::Close()
{
	if(!Opened) return;

	Sys.RemoveTask(_taskLink);

	OnClose();

	Linked	= false;
	Opened	= false;
}

bool NetworkInterface::Active() const
{
	return Opened && Linked && IP != IPAddress::Any();
}

void NetworkInterface::OnLoop()
{
	bool old	= Linked;
	// 未连接时执行连接
	if(!old)
	{
		bool link	= OnLink(++_retry);
		if(link ^ old)
		{
			debug_printf("%s::Change %s => %s \r\n", Name, old ? "连接" : "断开", link ? "连接" : "断开");
			Linked	= link;
			_retry	= 0;
		}
	}
	else
	{
		// 检测并通知状态改变
		bool link	= CheckLink();
		if(link ^ old)
		{
			debug_printf("%s::Change %s => %s \r\n", Name, old ? "连接" : "断开", link ? "连接" : "断开");
			Linked	= link;

			Changed(*this);
		}
	}
}

Socket* NetworkInterface::CreateClient(const NetUri& uri)
{
	if(!Active()) return nullptr;

	auto socket	= CreateSocket(uri.Type);
	if(socket)
	{
		socket->Local.Address	= uri.Address;
		socket->Local.Port		= uri.Port;
	}

	return socket;
}

Socket* NetworkInterface::CreateRemote(const NetUri& uri)
{
	if(!Active()) return nullptr;

	auto socket	= CreateSocket(uri.Type);
	if(socket)
	{
		socket->Local.Address	= IP;
		socket->Remote.Address	= uri.Address;
		socket->Remote.Port		= uri.Port;
		socket->Server			= uri.Host;
	}

	return socket;
}

void NetworkInterface::InitConfig()
{
	IPAddress defip(192, 168, 1, 1);

	// 随机IP，取ID最后一个字节
	IP = defip;
	byte first = Sys.ID[0];
	if(first <= 1 || first >= 254) first = 2;
	IP[3]	= first;

	Mask	= IPAddress(255, 255, 255, 0);
	Gateway	= defip;

	// 修改为双DNS方案，避免单点故障。默认使用阿里和百度公共DNS。
	DNSServer	= IPAddress(223, 5, 5, 5);
	DNSServer2	= IPAddress(180, 76, 76, 76);

	auto& mac	= Mac;
	// 随机Mac，前2个字节取自ASCII，最后4个字节取自后三个ID
	//mac[0] = 'W'; mac[1] = 'S';
	// 第一个字节最低位为1表示组播地址，所以第一个字节必须是偶数
	mac[0]	= 'N';
	mac[1]	= 'X';
	for(int i=0; i< 4; i++)
		mac[2 + i] = Sys.ID[3 - i];

	// 多网卡时MAC不能相同
	int k=0;
	for(;k<All.Count() && All[k]!=this;k++);
	mac[5]	+= k;

	Mode	= NetworkType::Wire;

	OnInitConfig();
}

bool NetworkInterface::LoadConfig()
{
	auto cfg	= Config::Current;
	if(!cfg) return false;

	auto nc	= (NetConfig*)cfg->Get(Name);
	if(!nc) return false;

	debug_printf("%s::Load %d 字节\r\n", Name, sizeof(NetConfig));

	IP			= nc->IP;
	Mask		= nc->Mask;
	Mac			= nc->Mac;
	Mode		= (NetworkType)nc->Mode;

	DNSServer	= nc->DNSServer;
	DNSServer2	= nc->DNSServer2;
	Gateway		= nc->Gateway;

	OnLoadConfig(Buffer(nc->Data, sizeof(nc->Data)));

	return true;
}

bool NetworkInterface::SaveConfig() const
{
	auto cfg	= Config::Current;
	if(!cfg) return false;

	NetConfig nc;
	nc.IP	= IP.Value;
	nc.Mask	= Mask.Value;
	Mac.CopyTo(nc.Mac);
	nc.Mode	= (byte)Mode;

	nc.DNSServer	= DNSServer.Value;
	nc.DNSServer2	= DNSServer2.Value;
	nc.Gateway		= Gateway.Value;

	Buffer dat(nc.Data, sizeof(nc.Data));
	dat.Clear();
	OnSaveConfig(dat);

	debug_printf("%s::Save %d 字节\r\n", Name, sizeof(nc));

	Buffer bs(&nc, sizeof(nc));
	return cfg->Set(Name, bs);
}

void NetworkInterface::ShowConfig() const
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
	//net_printf("\r\n    DHCP:\t");
	//DHCPServer.Show();
	net_printf("\r\n    DNS:\t");
	DNSServer.Show();
	if(!DNSServer2.IsAny())
	{
		net_printf("\r\n    DNS2:\t");
		DNSServer2.Show();
	}
	net_printf("\r\n    Speed:\t%dMps", Speed);
	net_printf("\r\n    Mode:\t");
	switch(Mode)
	{
		case NetworkType::Wire:
			net_printf("有线");
			break;
		case NetworkType::Station:
			net_printf("无线Station");
			break;
		case NetworkType::AP:
			net_printf("无线AP热点");
			break;
		case NetworkType::STA_AP:
			net_printf("无线Station+AP热点");
			break;
	}
	//net_printf("\r\n");

	OnShowConfig();

	net_printf("\r\n");
#endif
}

// DNS解析。默认仅支持字符串IP地址解析
IPAddress NetworkInterface::QueryDNS(const String& domain)
{
	return IPAddress::Parse(domain);
}

/****************************** WiFiInterface ************************************/

WiFiInterface::WiFiInterface() : NetworkInterface()
{
	Mode	= NetworkType::Station;

	SSID	= nullptr;
	Pass	= nullptr;
}

bool WiFiInterface::IsStation() const
{
	return Mode == NetworkType::Station || Mode == NetworkType::STA_AP;
}

bool WiFiInterface::IsAP() const
{
	return Mode == NetworkType::AP || Mode == NetworkType::STA_AP;
}

Socket* WiFiInterface::CreateClient(const NetUri& uri)
{
	// WiFi无需连接路由器就可以建立本地监听
	if(!Opened) return nullptr;

	auto socket	= CreateSocket(uri.Type);
	if(socket)
	{
		socket->Local.Address	= uri.Address;
		socket->Local.Port		= uri.Port;
	}

	return socket;
}

void WiFiInterface::OnInitConfig()
{
	if(!SSID)
		SSID	= new String();
	else
		SSID->Clear();

	if(!Pass)
		Pass	= new String();
	else
		Pass->Clear();
}

void WiFiInterface::OnLoadConfig(const Buffer& buf)
{
	Stream ms(buf);
	if(SSID) *SSID	= ms.ReadString();
	if(Pass) *Pass	= ms.ReadString();
}

void WiFiInterface::OnSaveConfig(Buffer& buf) const
{
	Stream ms(buf);
	if(SSID) ms.WriteArray(*SSID);
	if(Pass) ms.WriteArray(*Pass);
}

void WiFiInterface::OnShowConfig() const
{
	if(SSID) { net_printf("\r\n    SSID:\t"); SSID->Show(false); }
	if(Pass) { net_printf("\r\n    Pass:\t"); Pass->Show(false); }
}
