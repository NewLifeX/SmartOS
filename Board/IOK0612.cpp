#include "IOK0612.h"

#include "Kernel\Task.h"

#include "Device\Power.h"
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

	// 系统休眠时自动进入低功耗
	//Power::AttachTimeSleep();
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
		port->Index	= i;
		port->Press	= press;
		port->UsePress();
		port->Open();
		Buttons.Add(port);
	}
}

NetworkInterface* IOK0612::Create8266()
{
	// auto host	= new Esp8266(COM2, PB2, PA1);	// 触摸开关的
	auto host	= new Esp8266(COM2, PB12, PA1);

	// 初次需要指定模式 否则为 Wire
	bool join	= host->SSID && *host->SSID;
	//if (!join) host->Mode = NetworkType::AP;

	if (!join)
	{
		*host->SSID	= "WSWL";
		*host->Pass = "12345678";

		host->Mode	= NetworkType::STA_AP;
	}

	Client->Register("SetWiFi", &Esp8266::SetWiFi, host);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, host);

	host->Open();

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
	Client->Register("Gateway/Restart", &TokenClient::InvokeRestart, Client);
	// 重置
	Client->Register("Gateway/Reset", &TokenClient::InvokeReset, Client);
	// 设置远程地址
	Client->Register("Gateway/SetRemote", &TokenClient::InvokeSetRemote, Client);
	// 获取远程配置信息
	Client->Register("Gateway/GetRemote", &TokenClient::InvokeGetRemote, Client);
	// 获取所有Ivoke命令
	Client->Register("Api/All", &TokenClient::InvokeGetAllApi, Client);

	InitAlarm();

	if (Data && Size > 0)
	{
		auto& ds = Client->Store;
		ds.Data.Set(Data, Size);
	}
}

void IOK0612::Register(int index, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(index, dp);
}

void IOK0612::InitNet()
{
	Host	= Create8266();
}

static void OnAlarm(AlarmItem& item)
{
	// 1长度n + 1类型 + 1偏移 + (n-2)数据
	auto bs	= item.GetData();
	debug_printf("OnAlarm ");
	bs.Show(true);

	if(bs[1] == 0x06)
	{
		auto client = IOK0612::Current->Client;
		client->Store.Write(bs[2], bs.Sub(3, bs[0] - 2));

		// 主动上报状态
		client->ReportAsync(bs[2], bs[0] - 2);
	}
}

void IOK0612::InitAlarm()
{
	if (!Client) return;

	if (!AlarmObj) AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::Set, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::Get, AlarmObj);

	AlarmObj->OnAlarm	= OnAlarm;
	AlarmObj->Start();
}

void IOK0612::Invoke(const String& ation, const Buffer& bs)
{
	if (!Client) return;

	Client->Invoke(ation, bs);

}

void IOK0612::Restore()
{
	if(Client) Client->Reset("按键重置");
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
	auto client	= IOK0612::Current->Client;
	if (time >= 5000 && time < 10000)
	{
		if(client) client->Reset("按键重置");
	}
	else if (time >= 3000)
	{
		if(client) client->Reboot("按键重启");
		Sys.Reboot(1000);
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
