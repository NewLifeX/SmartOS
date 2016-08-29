#include "IOK027X.h"

#include "Task.h"

#include "WatchDog.h"
#include "Config.h"

#include "Drivers\Esp8266\Esp8266.h"
#include "TokenNet\TokenController.h"
#include "Kernel\Task.h"

IOK027X* IOK027X::Current = nullptr;

IOK027X::IOK027X()
{
	LedPins.Add(PA0);
	LedPins.Add(PA4);

	LedsShow = false;
	LedsTaskId = 0;

	Host	= nullptr;	// 网络主机
	Client	= nullptr;

	Data	= nullptr;
	Size	= 0;
	Current = this;
}

void IOK027X::Init(ushort code, cstring name, COM message)
{
	auto& sys	= (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

	// RTC 提取时间
	auto Rtc = HardRTC::Instance();
	Rtc->LowPower = false;
	Rtc->External = false;
	Rtc->Init();
	Rtc->Start(false, false);

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
		port->Write(false);
		Leds.Add(port);
	}
}

ISocketHost* IOK027X::Create8266()
{
	auto host	= new Esp8266(COM2, PB2, PA1);

	// 初次需要指定模式 否则为 Wire
	bool join	= host->SSID && *host->SSID;
	//if (!join) host->Mode = SocketMode::AP;

	if (!join)
	{
		*host->SSID	= "WsLink";
		*host->Pass = "12345678";

		host->Mode	= SocketMode::STA_AP;
	}
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
	// 重启
	Client->Register("Gateway/Restart", &TokenClient::InvokeRestStart, Client);
	// 重置
	Client->Register("Gateway/Reset", &TokenClient::InvokeRestBoot, Client);

	if (Data && Size > 0)
	{
		auto& ds = Client->Store;
		ds.Data.Set(Data, Size);
	}

	// 如果若干分钟后仍然没有打开令牌客户端，则重启系统
	Sys.AddTask(
		[](void* p){
			auto & bsp = *(IOK027X*)p;
			auto & client = *bsp.Client;
			if(!client.Opened)
			{
				debug_printf("联网超时，准备重启Esp！\r\n\r\n");
				// Sys.Reset();
				auto port = dynamic_cast<Esp8266*>(bsp.Host);
				port->Close();
				Sys.Sleep(1000);
				port->Open();
			}
		},
		this, 8 * 60 * 1000, -1, "联网检查");
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

	//if(Client->Opened) return;

	debug_printf("\r\n OpenClient \r\n");

	auto esp	= dynamic_cast<Esp8266*>(&host);
	if(esp && !esp->Led && LedsShow) esp->SetLed(*Leds[0]);

	auto tk = TokenConfig::Current;

	// STA模式下，主连接服务器
	if (host.IsStation() && esp->Joined && !Client->Master) AddControl(host, tk->Uri(), 0);

	// STA或AP模式下，建立本地监听
	if(Client->Controls.Count() == 0)
	{
		NetUri uri(NetType::Udp, IPAddress::Broadcast(), 3355);
		AddControl(host, uri, tk->Port);
	}

	if (!Client->Opened)
		Client->Open();
	else
		Client->AttachControls();
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

void AlarmWrite(byte type, Buffer& bs)
{
	debug_printf("AlarmWrite type %d data ", type);
	bs.Show(true);

	auto client = IOK027X::Current->Client;

	Stream ms(bs);
	auto start = ms.ReadByte();
	Buffer data(bs.GetBuffer() + 1, bs.Length() - 1);

	client->Store.Write(start, data);
}

// void AlarmReport(byte type, Buffer&bs)
// {
// 	debug_printf("AlarmReport type %d data ", type);
// 	bs.Show(true);
// 
// 	Stream ms(bs);
// 	auto start = ms.ReadByte();
// 	auto size = ms.ReadByte();
// 	auto client = IOK027X::Current->Client;
// 
// 	client->ReportAsync(start, size);
// }

void AlarmDelayOpen(void *param)
{
	auto alarm = (Alarm*)param;

	if (DateTime::Now().Year > 2010)
	{
		alarm->Start();
		Sys.RemoveTask(Task::Current().ID);
	}
	else
		Sys.SetTask(Task::Current().ID, true, 2000);
}

void IOK027X::InitAlarm()
{
	if (!Client)return;

	if (!AlarmObj)AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::AlarmSet, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::AlarmGet, AlarmObj);

	AlarmObj->Register(5, AlarmWrite);
	// AlarmObj->Register(6, AlarmReport);

	if (DateTime::Now().Year > 2010)
		AlarmObj->Start();
	else
		Sys.AddTask(AlarmDelayOpen, AlarmObj, 2000, 2000, "打开Alarm");
}

void IOK027X::Restore()
{
	Config::Current->RemoveAll();

	Sys.Reset();
}

void IOK027X::FlushLed()
{
	bool stat = false;
	// 3分钟内 Client还活着则表示  联网OK
	if (Client && Client->LastActive + 180000 > Sys.Ms()&& LedsShow)stat = true;
	Leds[1]->Write(stat);
	if (!LedsShow)Sys.SetTask(LedsTaskId, false);
	// if (!LedsShow)Sys.SetTask(Task::Scheduler()->Current->ID, false);
}

bool IOK027X::LedStat(bool enable)
{
	auto esp = dynamic_cast<Esp8266*>(Host);
	if (esp)
	{
		if (enable)
		{
			esp->RemoveLed();
			esp->SetLed(*Leds[0]);
		}
		else
		{
			esp->RemoveLed();
			// 维护状态为false
			Leds[0]->Write(false);
		}
	}
	if (enable)
	{
		if (!LedsTaskId)
			LedsTaskId = Sys.AddTask(&IOK027X::FlushLed, this, 500, 500, "CltLedStat");
		else
			Sys.SetTask(LedsTaskId, true);
		LedsShow = true;
	}
	else
	{
		// 由任务自己结束，顺带维护输出状态为false
		// if (LedsTaskId)Sys.SetTask(LedsTaskId, false);
		LedsShow = false;
	}
	return LedsShow;
}

void IOK027X::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	ushort time = port->PressTime;
	if (time >= 6500 && time < 10000)
	{
		Sys.Sleep(1000);
		Sys.Reset();
		return;
	}

	if (time >= 9000 && time < 14000)
	{
		LedStat(!LedsShow);
		return;
	}

	if (time >= 14000)
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
