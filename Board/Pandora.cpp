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

PA0903::PA0903()
{
	LedPins.Add(PB1);
	LedPins.Add(PB14);

	Host = nullptr;
	Client = nullptr;
	ProxyFac = nullptr;
	AlarmObj = nullptr;

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

	auto host = new W5500(Spi1, PA8, PA0);

	return host;
}

NetworkInterface* PA0903::Create8266()
{
	auto host = new Esp8266(COM4, PE2, PD3);
	// 初次需要指定模式 否则为 Wire
	bool join = host->SSID && *host->SSID;
	// host->Mode = NetworkType::Station;
	// if (!join) host->Mode = NetworkType::AP;
	if (!join)
	{
		*host->SSID = "WSWL";
		*host->Pass = "12345678";

		host->Mode = NetworkType::STA_AP;
		host->WorkMode = NetworkType::STA_AP;
	}

	//Sys.AddTask(SetWiFiTask, this, 0, -1, "SetWiFi");
	Client->Register("SetWiFi", &Esp8266::SetWiFi, host);
	Client->Register("GetWiFi", &Esp8266::GetWiFi, host);
	host->Open();

	return host;
}

/******************************** Token ********************************/

void PA0903::InitClient()
{
	if (Client) return;

	auto tk = TokenConfig::Current;

	// 创建客户端
	auto client = new TokenClient();
	client->Cfg = tk;

	Client = client;

	InitAlarm();
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
}

void PA0903::Register(int index, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(index, dp);
}

//void OnInitNet(void* param)
//{
//	auto& bsp = *(PA0903*)param;
//
//	/*// 检查是否连接网线
//	auto host = (W5500*)bsp.Create5500();
//	// 软路由的DHCP要求很严格，必须先把自己IP设为0
//	host->IP = IPAddress::Any();
//
//	host->EnableDNS();
//	host->EnableDHCP();
//	bsp.Host = host;*/
//
//
//	
//}

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
		auto client = PA0903::Current->Client;
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

void PA0903::InitNet()
{
	Create5500();
	Create8266();
	Client->Open();
}

void PA0903::Restore()
{
	if (Client) Client->Reset("按键重置");
}

//auto host	= (W5500*)Create5500(Spi1, PA8, PA0, led);
