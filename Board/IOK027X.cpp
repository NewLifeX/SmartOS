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
	Host	= nullptr;	// 网络主机
	Client	= nullptr;
}

void IOK027X::Init(ushort code, cstring name, COM message, int baudRate)
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

ISocketHost* IOK027X::Create8266(bool apOnly)
{
	auto host	= new Esp8266(COM2,PB2,PA1);

	// 初次需要指定模式 否则为 Wire
	if (host->SSID->Length() == 0)
		host->Mode = SocketMode::AP;
// 	else
// 	{
// 		if (Client->Cfg->Token().Length() == 0)
// 			host->Mode = SocketMode::STA_AP;
// 		else
// 			host->Mode = SocketMode::Station;
// 	}

	// APOnly且不是AP模式时，强制AP模式  wifi开关不存在APOnly
	// if (apOnly && !host->IsAP()) host->WorkMode = SocketMode::AP;

	// 绑定委托，避免5500没有连接时导致没有启动客户端
	host->NetReady.Bind(&IOK027X::OpenClient, this);

	//Sys.AddTask(SetWiFiTask, this, 0, -1, "SetWiFi");
	Client->Register("SetWiFi", &Esp8266::SetWiFi, host);

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
	assert(Client, "Client");
	debug_printf("\r\n OpenClient \r\n");

	// auto esp = dynamic_cast<Esp8266*>(&host);

	auto tk = TokenConfig::Current;
	NetUri uri(NetType::Udp, IPAddress::Broadcast(), 3355);

	auto esp = dynamic_cast<Esp8266*>(&host);

	if (Host&&esp)
	{
		if (esp->IsStation())
		{
			debug_printf("IsStation Add remote\r\n");
			AddControl(*Host, tk->Uri(), 0);
		}
		if (esp->IsAP())
		{
			debug_printf("IsAP Add Local\r\n");
			AddControl(*Host, uri, tk->Port);
		}

		if (host.Mode != SocketMode::Wire)
			Client->Open();
		else
		{
			debug_printf("WIFI开关不存在Wire模式\r\n");
			Sys.Sleep(10);
			Sys.Reset();
		}
	}
}

TokenController* IOK027X::AddControl(ISocketHost& host, const NetUri& uri, ushort localPort)
{
	// 创建连接服务器的Socket
	auto socket = host.CreateRemote(uri);

	// 创建连接服务器的控制器
	auto ctrl = new TokenController();
	//ctrl->Port = dynamic_cast<ITransport*>(socket);
	ctrl->Socket = socket;

	// 创建客户端
	auto client = Client;
	if (localPort == 0)
		client->Master = ctrl;
	else
	{
		socket->Local.Port = localPort;
		ctrl->ShowRemote = true;
		client->Controls.Add(ctrl);
	}

	return ctrl;
}

void OnInitNet(void* param)
{
	auto& bsp = *(IOK027X*)param;

	// 没有接网线，需要完整WiFi通道
	auto esp = bsp.Create8266(false);
	bsp.Host = esp;
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
