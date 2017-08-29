#include "LinkBoard.h"

#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

LinkBoard::LinkBoard()
{
	Client = nullptr;

	Data = nullptr;
	Size = 0;
}

void* LinkBoard::InitData(void* data, int size)
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
	debug_printf("数据区[%d]：", hot->Times);
	Buffer(Data, Size).Show(true);
#endif

	return data;
}

// 写入数据区并上报
void LinkBoard::Write(uint offset, byte data)
{
	auto client = Client;
	if (!client) return;

	client->Store.Write(offset, data);
	client->ReportAsync(offset, 1);
}

/******************************** Link ********************************/

void LinkBoard::InitClient()
{
	if (Client) return;

	// 初始化配置区
	InitConfig();

	// 初始化令牌网
	auto tk = LinkConfig::Create("tcp://192.168.0.3:2233");

	// 创建客户端
	auto tc = LinkClient::CreateFast(Buffer(Data, Size));
	tc->Cfg = tk;
	tc->MaxNotActive = 8 * 60 * 1000;

	Client = tc;
}

void LinkBoard::Register(uint offset, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(offset, dp);
}

void LinkBoard::Register(uint offset, uint size, Handler hook)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(offset, size, hook);
}

void LinkBoard::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	auto client = Client;
	if (!client) return;

	ushort time = port->PressTime;
	if (time >= 5000 && time < 10000)
	{
		client->Reset("按键重置");
	}
	else if (time >= 3000)
	{
		client->Reboot("按键重启");
		Sys.Reboot(1000);
	}
}

NetworkInterface* LinkBoard::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto net = new W5500(Net.Spi, Net.Irq, Net.Reset);
	if (!net->Open())
	{
		delete net;
		return nullptr;
	}

	net->SetLed(*Leds[0]);
	net->EnableDNS();
	net->EnableDHCP();

	return net;
}

NetworkInterface* LinkBoard::Create8266()
{
	debug_printf("\r\nEsp8266::Create \r\n");

	auto esp = new Esp8266();
	esp->Init(Esp.Com, Esp.Baudrate);
	esp->Set(Esp.Power, Esp.Reset, Esp.LowPower);

	// 初次需要指定模式 否则为 Wire
	bool join = esp->SSID && *esp->SSID;
	if (!join)
	{
		*esp->SSID = "WSWL";
		*esp->Pass = "12345678";

		esp->Mode = NetworkType::STA_AP;
		esp->WorkMode = NetworkType::STA_AP;
	}

	if (!esp->Open())
	{
		delete esp;
		return nullptr;
	}

	esp->SetLed(*Leds[1]);
	//Client->Register("SetWiFi", &Esp8266::SetWiFi, esp);
	//Client->Register("GetWiFi", &Esp8266::GetWiFi, esp);

	return esp;
}
