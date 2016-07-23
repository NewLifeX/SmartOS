#include "IOK027X.h"

#include "Task.h"

#include "WatchDog.h"
#include "Config.h"

#include "Drivers\Esp8266\Esp8266.h"

#include "TokenNet\TokenController.h"

IOK027X::IOK027X()
{
	LedPins.Add(PA0);
	LedPins.Add(PA4);

	Host	= nullptr;	// 网络主机
	Client	= nullptr;

	Data	= nullptr;
	Size	= 0;
}

void IOK027X::Init(ushort code, cstring name, COM message)
{
	auto& sys	= (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

    // 初始化系统
    sys.Init();
#if DEBUG
    sys.MessagePort = message; // 指定printf输出的串口
    Sys.ShowInfo();

	WatchDog::Start(20000, 10000);
#endif

	// Flash最后一块作为配置区
	Config::Current	= &Config::CreateFlash();
}

void* IOK027X::InitData(void* data, int size)
{
	// 启动信息
	auto hot	= &HotConfig::Current();
	hot->Times++;

	data = hot->Next();
	if (hot->Times == 1)
	{
		Buffer ds(data, size);
		ds.Clear();
		ds[0] = size;
	}
	// Buffer bs(data, size);
	// debug_printf("HotConfig Times %d Data: ",hot->Times);
	// bs.Show(true);

	Data	= data;
	Size	= size;

	return data;
}

void IOK027X::InitLeds()
{
	for(int i=0; i<LedPins.Count(); i++)
	{
		auto port	= new OutputPort(LedPins[i]);
		port->Invert = true;
		port->Open();
		Leds.Add(port);
	}
}

ISocketHost* IOK027X::Create8266()
{
	auto host	= new Esp8266(COM2, PB2, PA1);

	// 初次需要指定模式 否则为 Wire
	bool join	= host->SSID && *host->SSID;
	if (!join) host->Mode	= SocketMode::AP;

	// 绑定委托，避免5500没有连接时导致没有启动客户端
	host->NetReady.Bind(&IOK027X::OpenClient, this);

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
	Client->MaxNotActive = 480000;

	if (Data && Size > 0)
	{
		auto& ds = Client->Store;
		ds.Data.Set(Data, Size);
	}

	// 如果若干分钟后仍然没有打开令牌客户端，则重启系统
	Sys.AddTask(
		[](void* p){
			auto& client	= *(TokenClient*)p;
			if(!client.Opened)
			{
				debug_printf("联网超时，准备重启系统！\r\n\r\n");
				Sys.Reset();
			}
		},
		client, 8 * 60 * 1000, -1, "联网检查");
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

	if(Client->Opened) return;

	debug_printf("\r\n OpenClient \r\n");

	auto esp	= dynamic_cast<Esp8266*>(&host);
	if(esp && !esp->Led) esp->SetLed(*Leds[0]);

	auto tk = TokenConfig::Current;

	// STA模式下，主连接服务器
	if (host.IsStation()) AddControl(host, tk->Uri(), 0);

	// STA或AP模式下，建立本地监听
	NetUri uri(NetType::Udp, IPAddress::Broadcast(), 3355);
	AddControl(host, uri, tk->Port);

	Client->Open();
}

TokenController* IOK027X::AddControl(ISocketHost& host, const NetUri& uri, ushort localPort)
{
	// 创建连接服务器的Socket
	auto socket	= host.CreateRemote(uri);

	// 创建连接服务器的控制器
	auto ctrl	= new TokenController();
	//ctrl->Port = dynamic_cast<ITransport*>(socket);
	ctrl->Socket	= socket;

	// 创建客户端
	auto client	= Client;
	if(localPort == 0)
		client->Master	= ctrl;
	else
	{
		socket->Local.Port	= localPort;
		ctrl->ShowRemote	= true;
		client->Controls.Add(ctrl);
	}

	return ctrl;
}

void IOK027X::InitNet()
{
	Host	= Create8266();
}

void IOK027X::Restore()
{
	Config::Current->RemoveAll();

	Sys.Reset();
}

void IOK027X::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);
	// Sys.Sleep(500);

	if (port->PressTime >= 10000)
	{
		Sys.Reset();
		return;
	}
	if (port->PressTime >= 15000)
	{
		Restore();
		return;
	}
}

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
