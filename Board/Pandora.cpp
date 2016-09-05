#include "Pandora.h"

#include "Task.h"

#include "WatchDog.h"
#include "Config.h"

#include "Device\Spi.h"
#include "Drivers\W5500.h"
#include "TokenNet\TokenController.h"

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

ISocketHost* PA0903::Create5500()
{
	debug_printf("\r\nW5500::Create \r\n");

	auto host = new W5500(Spi1, PA8, PA0);
	host->NetReady.Bind(&PA0903::OpenClient, this);

	return host;
}

void PA0903::InitClient()
{
	if (Client) return;

	auto tk = TokenConfig::Current;

	// 创建客户端
	auto client = new TokenClient();
	client->Cfg = tk;

	Client = client;

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
		[](void* p) {
		if (!((TokenClient*)p)->Opened) Sys.Reboot();
	},
		client, 8 * 60 * 1000, -1, "CheckClient");
}

void PA0903::Register(int index, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(index, dp);
}

void PA0903::OpenClient(ISocketHost& host)
{
	assert(Client, "Client");

	debug_printf("\r\n OpenClient \r\n");

	// 网络就绪后，打开指示灯
	auto net = dynamic_cast<W5500*>(&host);
	if (net && !net->Led) net->SetLed(*Leds[0]);

	auto tk = TokenConfig::Current;
	NetUri uri(NetType::Udp, IPAddress::Broadcast(), 3355);

	// 避免重复打开  不判断也行  这里只有W5500
	if (!Client->Opened)
	{
		AddControl(*Host, tk->Uri(), 0);
		AddControl(*Host, uri, tk->Port);
		Client->Open();
		if (ProxyFac)ProxyFac->AutoStart();
	}
}

TokenController* PA0903::AddControl(ISocketHost& host, const NetUri& uri, ushort localPort)
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
	auto& bsp = *(PA0903*)param;

	// 检查是否连接网线
	auto host = (W5500*)bsp.Create5500();
	// 软路由的DHCP要求很严格，必须先把自己IP设为0
	host->IP = IPAddress::Any();

	host->EnableDNS();
	host->EnableDHCP();
	bsp.Host = host;
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

void AlarmWrite(byte type, Buffer& bs)
{
	debug_printf("AlarmWrite type %d data ", type);
	bs.Show(true);

	auto client = PA0903::Current->Client;

	Stream ms(bs);
	auto start = ms.ReadByte();
	Buffer data(bs.GetBuffer() + 1, bs.Length() - 1);

	client->Store.Write(start, data);
}

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

void PA0903::InitAlarm()
{
	if (!Client)return;

	if (!AlarmObj)AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::AlarmSet, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::AlarmGet, AlarmObj);

	AlarmObj->Register(5, AlarmWrite);

	if (DateTime::Now().Year > 2010)
		AlarmObj->Start();
	else
		Sys.AddTask(AlarmDelayOpen, AlarmObj, 2000, 2000, "打开Alarm");
}

void PA0903::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
}

void PA0903::Restore()
{
	Config::Current->RemoveAll();

	Sys.Reboot();
}

//auto host	= (W5500*)Create5500(Spi1, PA8, PA0, led);
