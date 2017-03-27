#include "NH3_0317.h"

#include "Kernel\Task.h"

#include "Device\Power.h"
#include "Device\WatchDog.h"
#include "Config.h"

#include "Drivers\Esp8266\Esp8266.h"
#include "TokenNet\TokenController.h"
#include "Kernel\Task.h"

NH3_0317* NH3_0317::Current = nullptr;

NH3_0317::NH3_0317()
{
	//LedPins.Add(PA4);
	ButtonPins.Add(PA0);
	LedPins.Add(PB0);

	LedsShow = false;
	LedsTaskId = 0;

	Host	= nullptr;	// 网络主机
	Client	= nullptr;

	Data	= nullptr;
	Size	= 0;
	Current = this;
}

void NH3_0317::Init(ushort code, cstring name, COM message)
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

void* NH3_0317::InitData(void* data, int size)
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

void NH3_0317::InitLeds()
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

/*static void ButtonOnpress(InputPort* port, bool down, void* param)
{
	if (port->PressTime > 1000)
		NH3_0317::OnLongPress(port, down);
}*/

void NH3_0317::InitButtons(const Delegate2<InputPort&, bool>& press)
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

NetworkInterface* NH3_0317::Create8266()
{
	// auto esp	= new Esp8266(COM2, PB2, PA1);	// 触摸开关的
	//auto esp	= new Esp8266(COM3, P0, PA5);
	auto esp = new Esp8266();
	esp->Init(COM3);
	esp->Set(P0, PA5);

	// 初次需要指定模式 否则为 Wire
	bool join	= esp->SSID && *esp->SSID;
	//if (!join) esp->Mode = NetworkType::AP;

	if (!join)
	{
		*esp->SSID	= "WSWL";
		*esp->Pass = "12345678";

		esp->Mode	= NetworkType::STA_AP;
	}

	if(!esp->Open())
	{
		delete esp;
		return nullptr;
	}

	esp->SetLed(*Leds[0]);
	Client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, esp);

	return esp;
}

/******************************** Token ********************************/

void NH3_0317::SetStore(void*data, int len)
{
	if (!Client)return;
	Client->Store.Data.Set(data, len);
}

void NH3_0317::InitClient()
{
	if (Client) return;

	// 初始化令牌网
	auto tk = TokenConfig::Create("smart.wslink.cn", NetType::Udp, 33333, 3377);

	// 创建客户端
	auto tc = TokenClient::CreateFast(Buffer(Data, Size));
	tc->Cfg = tk;
	tc->MaxNotActive = 8 * 60 * 1000;
	tc->UseLocal();

	Client = tc;
}

void NH3_0317::Register(int index, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(index, dp);
}

void NH3_0317::InitNet()
{
	Host	= Create8266();
	Client->Open();
}

static void OnAlarm(AlarmItem& item)
{
	// 1长度n + 1类型 + 1偏移 + (n-2)数据
	auto bs	= item.GetData();
	debug_printf("OnAlarm ");
	bs.Show(true);

	if(bs[1] == 0x06)
	{
		auto client = NH3_0317::Current->Client;
		client->Store.Write(bs[2], bs.Sub(3, bs[0] - 2));

		// 主动上报状态
		client->ReportAsync(bs[2], bs[0] - 2);
	}
}

void NH3_0317::InitAlarm()
{
	if (!Client) return;

	if (!AlarmObj) AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::Set, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::Get, AlarmObj);

	AlarmObj->OnAlarm	= OnAlarm;
	AlarmObj->Start();
}

void NH3_0317::Invoke(const String& ation, const Buffer& bs)
{
	if (!Client) return;

	Client->Invoke(ation, bs);

}

void NH3_0317::Restore()
{
	if(Client) Client->Reset("按键重置");
}

void NH3_0317::FlushLed()
{
	bool stat = false;
	// 3分钟内 Client还活着则表示  联网OK
	if (Client && Client->LastActive + 180000 > Sys.Ms()&& LedsShow)stat = true;
	Leds[1]->Write(stat);
	if (!LedsShow)Sys.SetTask(LedsTaskId, false);
	// if (!LedsShow)Sys.SetTask(Task::Scheduler()->Current->ID, false);
}

bool NH3_0317::LedStat(bool enable)
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
			LedsTaskId = Sys.AddTask(&NH3_0317::FlushLed, this, 500, 500, "CltLedStat");
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

void NH3_0317::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	ushort time = port->PressTime;
	auto client	= NH3_0317::Current->Client;
	if (time >= 5000 && time < 10000)
	{
		if(client) client->Reset("按键重置");
	}
	else if (time >= 1000)
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
