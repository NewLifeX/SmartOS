#include "Pandora.h"

#include "Kernel\Task.h"

#include "Device\WatchDog.h"
#include "Config.h"

#include "Device\Spi.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"
#include "TokenNet\TokenController.h"

#include "Device\RTC.h"

PA0903* PA0903::Current = nullptr;
static TokenClient*	Client = nullptr;	// 令牌客户端

PA0903::PA0903()
{
	LedPins.Add(PB1);
	LedPins.Add(PB14);

	Client	= nullptr;
	ProxyFac	= nullptr;
	AlarmObj	= nullptr;

	Data = nullptr;
	Size = 0;
	Current = this;
}

void PA0903::Init(ushort code, cstring name, COM message)
{
	auto& sys = (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

	// RTC 提取时间
	HardRTC::Start(false, false);

	// 初始化系统
	sys.Init();
#if DEBUG
	sys.MessagePort = message; // 指定printf输出的串口
	Sys.ShowInfo();

	WatchDog::Start(20000, 10000);
#else
	WatchDog::Start();

	// 系统休眠时自动进入低功耗
	Power::AttachTimeSleep();
#endif

	// Flash最后一块作为配置区
	Config::Current = &Config::CreateFlash();
}

void* PA0903::InitData(void* data, int size)
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

void PA0903::InitLeds()
{
	for (int i = 0; i < LedPins.Count(); i++)
	{
		auto port = new OutputPort(LedPins[i]);
		port->Open();
		Leds.Add(port);
	}
}

NetworkInterface* PA0903::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto net = new W5500(Spi1, PA8, PA0);
	if(!net->Open())
	{
		delete net;
		return nullptr;
	}

	net->EnableDNS();
	net->EnableDHCP();

	return net;
}

NetworkInterface* PA0903::Create8266()
{
	auto esp = new Esp8266(COM4, PE2, PD3);
	// 初次需要指定模式 否则为 Wire
	bool join = esp->SSID && *esp->SSID;
	// esp->Mode = NetworkType::Station;
	// if (!join) esp->Mode = NetworkType::AP;
	if (!join)
	{
		*esp->SSID = "WSWL";
		*esp->Pass = "12345678";

		esp->Mode = NetworkType::STA_AP;
		esp->WorkMode = NetworkType::STA_AP;
	}

	if(!esp->Open())
	{
		delete esp;
		return nullptr;
	}

	Client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, esp);

	return esp;
}

/******************************** Token ********************************/

void PA0903::InitClient()
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

void PA0903::Register(int index, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(index, dp);
}

void OnInitNet(void* param)
{
	auto& bsp	= *(PA0903*)param;

	bsp.Create5500();
	bsp.Create8266();

	Client->Open();
}

void PA0903::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
}

void  PA0903::InitProxy()
{
	if (ProxyFac)return;
	if (!Client)
	{
		debug_printf("请先初始化TokenClient！！\r\n");
		return;
	}
	ProxyFac = ProxyFactory::Create();

	ProxyFac->Register(new ComProxy(COM1));

	ProxyFac->Open(Client);
	// ProxyFac->AutoStart();		// 自动启动的设备  需要保证Client已经开启，否则没有意义
}

static void OnAlarm(AlarmItem& item)
{
	// 1长度n + 1类型 + 1偏移 + (n-2)数据
	auto bs = item.GetData();
	debug_printf("OnAlarm ");
	bs.Show(true);

	if (bs[1] == 0x06)
	{
		auto client = Client;
		client->Store.Write(bs[2], bs.Sub(3, bs[0] - 2));

		// 主动上报状态
		client->ReportAsync(bs[2], bs[0] - 2);
	}
}

void PA0903::InitAlarm()
{
	if (!Client)return;

	if (!AlarmObj) AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::Set, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::Get, AlarmObj);

	AlarmObj->OnAlarm = OnAlarm;
	AlarmObj->Start();
}

void PA0903::Restore()
{
	if (Client) Client->Reset("按键重置");
}

//auto host	= (W5500*)Create5500(Spi1, PA8, PA0, led);
