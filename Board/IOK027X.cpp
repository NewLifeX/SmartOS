#include "IOK027X.h"

#include "Task.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

#include "Net\Dhcp.h"
#include "Net\DNS.h"

#include "TokenNet\TokenController.h"
#include "App\FlushPort.h"


IOK027X::IOK027X()
{
	HostAP	= nullptr;	// 网络主机
	Client	= nullptr;
}

void IOK027X::Setup(ushort code, cstring name, COM message, int baudRate)
{
	auto& sys	= (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

    // 初始化系统
    //Sys.Clock = 48000000;
    sys.Init();
#if DEBUG
    sys.MessagePort = message; // 指定printf输出的串口
    Sys.ShowInfo();
#endif

	// WatchDog::Start();
	// Flash最后一块作为配置区
	Config::Current	= &Config::CreateFlash();
}

// 网络就绪
void OnNetReady(IOK027X& ap, ISocketHost& host)
{
	if (ap.Client) ap.Client->Open();
}

void* IOK027X::InitData(void* data, int size)
{
	// 启动信息
	auto hot = &HotConfig::Current();
	hot->Times++;

	data = hot->Next();
	if (hot->Times == 1)
	{
		Buffer ds(data, size);
		ds.Clear();
		ds[0] = size;
	}

	Data = data;
	Size = size;

	return data;
}


static void SetWiFiTask(void* param)
{
	auto ap	= (IOK027X*)param;
	auto client	= ap->Client;
	auto esp	= (Esp8266*)ap->HostAP;

	client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
}

ISocketHost* IOK027X::Create8266(bool apOnly)
{
	auto host	= new Esp8266(COM2,PB2,PA1);

	host->NetReady.Bind(&IOK027X::OpenClient, this);

	Sys.AddTask(SetWiFiTask, this, 0, -1, "SetWiFi");

	host->OpenAsync();

	return host;
}


/******************************** Token ********************************/

void IOK027X::InitClient()
{
	if (Client) return;

	auto tk = TokenConfig::Current;

	// 创建客户端
	auto client = new TokenClient();
	client->Cfg = tk;

	Client = client;

	if (Data && Size > 0)
	{
		auto& ds = Client->Store;
		ds.Data.Set(Data, Size);
	}
}

void IOK027X::Register(int index, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(index, dp);
}

void IOK027X::OpenClient(ISocketHost& host)
{
	debug_printf("\r\n OpenClient \r\n");
	assert(HostAP, "HostAP");
	assert(Client, "Client");

	auto tk = TokenConfig::Current;
	AddControl(*HostAP, tk->Uri());

	NetUri uri(NetType::Udp, IPAddress::Broadcast(), 3355);
	auto socket = AddControl(*HostAP, uri);
	socket->Local.Port = tk->Port;

	 if (HostAP)
	{
	 	socket = AddControl(*HostAP, uri);
	 	socket->Local.Port = tk->Port;
	 }

	Client->Open();
}

ISocket* IOK027X::AddControl(ISocketHost& host, const NetUri& uri)
{
	// 创建连接服务器的Socket
	auto socket = host.CreateRemote(uri);

	// 创建连接服务器的控制器
	auto ctrl = new TokenController();
	//ctrl->Port = dynamic_cast<ITransport*>(socket);
	ctrl->Socket = socket;

	// 创建客户端
	auto client = Client;
	client->Controls.Add(ctrl);

	// 如果不是第一个，则打开远程
	if (client->Controls.Count() > 1) ctrl->ShowRemote = true;

	return socket;
}

void OnInitNet(void* param)
{
	/*
	网络使用流程：

	5500网线检测
	网线连通
	启动DHCP
	作为Host
	Host为空 或 AP/STA_AP
	创建8266，加载配置
	Host为空
	作为Host
	else STA_AP
	工作模式 = AP

	令牌客户端主通道
	令牌客户端内网通道
	*/

	auto& ap = *(IOK027X*)param;

	// 如果Host已存在，则8266仅作为AP
	ap.HostAP = ap.Create8266(true);


	// 打开DHCP，完成时会打开客户端
	ap.HostAP->EnableDHCP();


	//auto& ap = *(IOK027X*)param;
	//auto host = (W5500*)ap.Create5500();
	//if (host->Open())
	//{
	//	host->EnableDNS();
	//	ap.Host = host;
	//}
	//else
	//{
	//	delete host;
	//}
	//if (!ap.Host || host->IsAP())
	//{
	//	// 如果Host已存在，则8266仅作为AP
	//	auto esp = ap.Create8266(ap.Host);
	//	if (esp)
	//	{
	//		if (!ap.Host)
	//			ap.Host = esp;
	//		else
	//			ap.HostAP = esp;
	//	}
	//}
	//
	//// 打开DHCP，完成时会打开客户端
	//if (ap.Host) ap.Host->EnableDHCP();
}


void IOK027X::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
}
/******************************** 2401 ********************************/

/*int Fix2401(const Buffer& bs)
{
	//auto& bs	= *(Buffer*)param;
	// 微网指令特殊处理长度
	uint rs	= bs.Length();
	if(rs >= 8)
	{
		rs = bs[5] + 8;
		//if(rs < bs.Length()) bs.SetLength(rs);
	}
	return rs;
}

ITransport* IOK027X::Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
{
	debug_printf("\r\n Create2401 \r\n");

	static Spi spi(spi_, 10000000, true);
	static NRF24L01 nrf;
	nrf.Init(&spi, ce, irq, power);

	auto tc	= TinyConfig::Create();
	if(tc->Channel == 0)
	{
		tc->Channel	= 120;
		tc->Speed	= 250;
	}
	if(tc->Interval == 0)
	{
		tc->Interval= 40;
		tc->Timeout	= 1000;
	}

	nrf.AutoAnswer	= false;
	nrf.DynPayload	= false;
	nrf.Channel		= tc->Channel;
	//nrf.Channel		= 120;
	nrf.Speed		= tc->Speed;

	nrf.FixData	= Fix2401;

	if(WirelessLed) net->Led	= CreateFlushPort(WirelessLed);

	nrf.Master	= true;

	return &nrf;
}*/

/*
NRF24L01+ 	(SPI3)		|	W5500		(SPI2)		|	TOUCH		(SPI3)
NSS			|				NSS			|	PD6			NSS
CLK			|				SCK			|				SCK
MISO		|				MISO		|				MISO
MOSI		|				MOSI		|				MOSI
PE3			IRQ			|	PE1			INT(IRQ)	|	PD11		INT(IRQ)
PD12		CE			|	PD13		NET_NRST	|				NET_NRST
PE6			POWER		|				POWER		|				POWER

ESP8266		(COM4)
TX
RX
PD3			RST
PE2			POWER

TFT
FSMC_D 0..15		TFT_D 0..15
NOE					RD
NWE					RW
NE1					RS
PE4					CE
PC7					LIGHT
PE5					RST

PE13				KEY1
PE14				KEY2

PE15				LED2
PD8					LED1


USB
PA11 PA12
*/
