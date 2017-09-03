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

#if DEBUG
	debug_printf("数据区%d：", hot->Times);
	Buffer(Data, Size).Show(true);
#endif

	return data;
}

// 写入数据区并上报
void PA0903::Write(uint offset, byte data)
{
	auto client = Client;
	if (!client) return;

	client->Store.Write(offset, data);
	client->ReportAsync(offset, 1);
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

void PA0903::InitButtons(const Delegate2<InputPort&, bool>& press)
{
	for (int i = 0; i < ButtonPins.Count(); i++)
	{
		auto port = new InputPort();
		port->Index = i;
		port->Init(ButtonPins[i], true);
		//port->ShakeTime = 40;
		port->Press = press;
		port->UsePress();
		port->Open();

		Buttons.Add(port);
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

	net->SetLed(*Leds[0]);
	net->EnableDNS();
	net->EnableDHCP();

	return net;
}

NetworkInterface* PA0903::Create8266()
{
	auto esp = new Esp8266();
	esp->Init(COM4);
	esp->Set(PE2, PD3);
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

	esp->SetLed(*Leds[1]);
	Client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, esp);

	return esp;
}

/******************************** Token ********************************/

void PA0903::InitClient()
{
	if (Client) return;

	// 创建客户端
	auto tc = TokenClient::Create("udp://smart.wslink.cn:33333", Buffer(Data, Size));
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


void PA0903::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	ushort time = port->PressTime;
	auto client = Client;
	if (time >= 5000 && time < 10000)
	{
		if (client) client->Reset("按键重置");
	}
	else if (time >= 3000)
	{
		if (client) client->Reboot("按键重启");
		Sys.Reboot(1000);
	}
}
