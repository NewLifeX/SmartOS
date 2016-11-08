#include "IOK0612.h"

#include "Kernel\Task.h"

#include "Device\WatchDog.h"
#include "Config.h"

#include "Drivers\Esp8266\Esp8266.h"
#include "TokenNet\TokenController.h"
#include "Kernel\Task.h"

IOK0612* IOK0612::Current = nullptr;

IOK0612::IOK0612()
{
	//LedPins.Add(PA4);
	ButtonPins.Add(PB6);
	LedPins.Add(PB7);

	LedsShow = false;
	LedsTaskId = 0;

	Host	= nullptr;	// 网络主机
	Client	= nullptr;

	Data	= nullptr;
	Size	= 0;
	Current = this;
}

void IOK0612::Init(ushort code, cstring name, COM message)
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
#else
	WatchDog::Start();
#endif

	// Flash最后一块作为配置区
	Config::Current	= &Config::CreateFlash();
}

void* IOK0612::InitData(void* data, int size)
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

void IOK0612::InitLeds()
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

void ButtonOnpress(InputPort* port, bool down, void* param)
{
	if (port->PressTime > 1000)
		IOK0612::OnLongPress(port, down);
}

void IOK0612::InitButtons(const Delegate2<InputPort&, bool>& press)
{
	for (int i = 0; i < ButtonPins.Count(); i++)
	{
		auto port = new InputPort(ButtonPins[i]);
		port->Invert = true;
		port->State	= i;
		port->Press	= press;
		port->UsePress();
		port->Open();
		Buttons.Add(port);
	}
}

ISocketHost* IOK0612::Create8266()
{
	// auto host	= new Esp8266(COM2, PB2, PA1);	// 触摸开关的
	auto host	= new Esp8266(COM2, PB12, PA1);

	// 初次需要指定模式 否则为 Wire
	bool join	= host->SSID && *host->SSID;
	//if (!join) host->Mode = SocketMode::AP;

	if (!join)
	{
		*host->SSID	= "WSWL";
		*host->Pass = "12345678";

		host->Mode	= SocketMode::STA_AP;
	}
	// 绑定委托，避免5500没有连接时导致没有启动客户端
	host->NetReady.Bind(&IOK0612::OpenClient, this);

	Client->Register("SetWiFi", &Esp8266::SetWiFi, host);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, host);

	host->OpenAsync();

	return host;
}

/******************************** Token ********************************/

void IOK0612::InitClient()
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
	// 设置远程地址
	Client->Register("Gateway/SetRemote", &TokenClient::InvokeSetRemote, Client);
	// 获取远程配置信息
	Client->Register("Gateway/GetRemote", &TokenClient::InvokeGetRemote, Client);
	// 获取所有Ivoke命令
	Client->Register("Api/All", &TokenClient::InvokeGetAllApi, Client);

	if (Data && Size > 0)
	{
		auto& ds = Client->Store;
		ds.Data.Set(Data, Size);
	}

	// 如果若干分钟后仍然没有打开令牌客户端，则重启系统
	Sys.AddTask(
		[](void* p){
			auto & bsp = *(IOK0612*)p;
			auto & client = *bsp.Client;
			if(!client.Opened)
			{
				debug_printf("联网超时，准备重启Esp！\r\n\r\n");
				// Sys.Reboot();
				auto port = dynamic_cast<Esp8266*>(bsp.Host);
				port->Close();
				Sys.Sleep(1000);
				port->Open();
			}
		},
		this, 8 * 60 * 1000, -1, "联网检查");
}

void IOK0612::Register(int index, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(index, dp);
}

void IOK0612::OpenClient(ISocketHost& host)
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

TokenController* IOK0612::AddControl(ISocketHost& host, const NetUri& uri, ushort localPort)
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

void IOK0612::InitNet()
{
	Host	= Create8266();
}

void AlarmWrite(byte type, Buffer& bs)
{
	debug_printf("AlarmWrite type %d data ", type);
	bs.Show(true);

	auto client = IOK0612::Current->Client;

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
// 	auto client = IOK0612::Current->Client;
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

void IOK0612::InitAlarm()
{
	if (!Client)return;

	if (!AlarmObj)AlarmObj = new Alarm();
	Client->Register("Policy/Set", &Alarm::Set, AlarmObj);
	Client->Register("Policy/Get", &Alarm::Get, AlarmObj);

	AlarmObj->Register(5, AlarmWrite);
	// AlarmObj->Register(6, AlarmReport);

	if (DateTime::Now().Year > 2010)
		AlarmObj->Start();
	else
		Sys.AddTask(AlarmDelayOpen, AlarmObj, 2000, 2000, "打开Alarm");
}

void IOK0612::Restore()
{
	if (!Client) return;
	Client->Reset();
}

void IOK0612::FlushLed()
{
	bool stat = false;
	// 3分钟内 Client还活着则表示  联网OK
	if (Client && Client->LastActive + 180000 > Sys.Ms()&& LedsShow)stat = true;
	Leds[1]->Write(stat);
	if (!LedsShow)Sys.SetTask(LedsTaskId, false);
	// if (!LedsShow)Sys.SetTask(Task::Scheduler()->Current->ID, false);
}

bool IOK0612::LedStat(bool enable)
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
			LedsTaskId = Sys.AddTask(&IOK0612::FlushLed, this, 500, 500, "CltLedStat");
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

void IOK0612::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	ushort time = port->PressTime;
	if (time >= 6500 && time < 10000)
	{
		Sys.Sleep(1000);
		Sys.Reboot();
		return;
	}	
	if (time >= 5000)
	{
		IOK0612::Current->Restore();
		return;
	}
}

/*
NRF24L01+ 	(SPI3)	
NSS			|		
CLK			|		
MISO		|		
MOSI		|		
PE3			IRQ		
PD12		CE		
PE6			POWER	

ESP8266		(COM4)
TX
RX
PD3			RST
PE2			POWER

LED1
LED2

KEY1
KEY2

*/
