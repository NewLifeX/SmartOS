#include "AP0803.h"

#include "Kernel\Task.h"

#include "Device\WatchDog.h"
#include "Config.h"

#include "Drivers\GSM07.h"

#include "TokenNet\TokenController.h"
#include "TokenNet\TokenConfig.h"
#include "TokenNet\TokenClient.h"

#include "Device\RTC.h"

#include "Message\ProxyFactory.h"

AP0803* AP0803::Current	= nullptr;

static TokenClient*	Client	= nullptr;	// 令牌客户端
static ProxyFactory*	ProxyFac	= nullptr;	// 透传管理器

AP0803::AP0803()
{
	LedPins.Add(PD8);
	LedPins.Add(PE15);
	ButtonPins.Add(PE13);
	ButtonPins.Add(PE14);

	Client	= nullptr;
	ProxyFac	= nullptr;
	AlarmObj	= nullptr;

	Data	= nullptr;
	Size	= 0;

	Current	= this;
}

void AP0803::Init(ushort code, cstring name, COM message)
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
	// Power::AttachTimeSleep();
#endif

	// Flash最后一块作为配置区
	Config::Current = &Config::CreateFlash();
}

void* AP0803::InitData(void* data, int size)
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

void AP0803::SetStore(void*data, int len)
{
	if (!Client)return;

	Client->Store.Data.Set(data, len);
}

void AP0803::InitLeds()
{
	for (int i = 0; i < LedPins.Count(); i++)
	{
		auto port = new OutputPort(LedPins[i]);
		port->Open();
		Leds.Add(port);
	}
}

/*static void ButtonOnpress(InputPort* port, bool down, void* param)
{
	if (port->PressTime > 1000)
		AP0803::Current->OnLongPress(port, down);
}*/

void AP0803::InitButtons(const Delegate2<InputPort&, bool>& press)
{
	for (int i = 0; i < ButtonPins.Count(); i++)
	{
		auto port = new InputPort(ButtonPins[i]);
		port->Invert = true;
		port->ShakeTime = 40;
		port->Index	= i;
		port->Press	= press;
		port->UsePress();
		port->Open();
		Buttons.Add(port);
	}
}

NetworkInterface* AP0803::CreateGPRS()
{
	debug_printf("\r\nCreateGPRS::Create \r\n");

	auto net = new GSM07();
	net->Init(COM4);
	net->Set(P0, P0);
	if(!net->Open())
	{
		delete net;
		return nullptr;
	}

	return net;
}

/******************************** Token ********************************/

void AP0803::InitClient()
{
	if (Client) return;

	// 初始化令牌网
	auto tk = TokenConfig::Create("smart.wslink.cn", NetType::Udp, 33333, 3377);

	// 创建客户端
	auto tc = TokenClient::CreateFast(Buffer(Data, Size));
	tc->Cfg = tk;
	tc->MaxNotActive = 8 * 60 * 1000;

	Client = tc;

	InitAlarm();
}

void AP0803::Register(uint offset, IDataPort& dp)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(offset, dp);
}

void AP0803::Register(uint offset, uint size, Handler hook)
{
	if (!Client) return;

	auto& ds = Client->Store;
	ds.Register(offset, size, hook);
}

static void OnInitNet(void* param)
{
	auto& bsp	= *(AP0803*)param;

	bsp.CreateGPRS();

	Client->Open();
}

void AP0803::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
}

void AP0803::Invoke(const String& ation, const Buffer& bs)
{
	if (!Client) return;

	Client->Invoke(ation, bs);

}

void  AP0803::InitProxy()
{
	if (ProxyFac)return;
	if (!Client)
	{
		debug_printf("请先初始化TokenClient！！\r\n");
		return;
	}
	ProxyFac = ProxyFactory::Create();

	ProxyFac->Register(new ComProxy(COM2));

	ProxyFac->Open(Client);
	// ProxyFac->AutoStart();		// 自动启动的设备  需要保证Client已经开启，否则没有意义
}

static void OnAlarm(AlarmItem& item)
{
	// 1长度n + 1类型 + 1偏移 + (n-2)数据
	auto bs	= item.GetData();
	debug_printf("OnAlarm ");
	bs.Show(true);

	if(bs[1] == 0x06)
	{
		auto client = Client;
		client->Store.Write(bs[2], bs.Sub(3, bs[0] - 2));

		// 主动上报状态
		client->ReportAsync(bs[2], bs[0] - 2);
	}
}

void AP0803::InitAlarm()
{
	if (!Client)return;

	if (!AlarmObj) AlarmObj = new Alarm();
	Client->Register("Policy/AlarmSet", &Alarm::Set, AlarmObj);
	Client->Register("Policy/AlarmGet", &Alarm::Get, AlarmObj);

	AlarmObj->OnAlarm	= OnAlarm;
	AlarmObj->Start();
}

/******************************** 2401 ********************************/

/*int Fix2401(const Buffer& bs)
{
	//auto& bs	= *(Buffer*)param;
	// 微网指令特殊处理长度
	uint rs	= bs.Length();
	if(rs >= 8)
	{
		rs = bs[5] + 8;
		//if(rs < bs.Length()) bs.SetLength(rs);
	}
	return rs;
}

ITransport* AP0803::Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
{
	debug_printf("\r\n Create2401 \r\n");

	static Spi spi(spi_, 10000000, true);
	static NRF24L01 nrf;
	nrf.Init(&spi, ce, irq, power);

	auto tc	= TinyConfig::Create();
	if(tc->Channel == 0)
	{
		tc->Channel	= 120;
		tc->Speed	= 250;
	}
	if(tc->Interval == 0)
	{
		tc->Interval= 40;
		tc->Timeout	= 1000;
	}

	nrf.AutoAnswer	= false;
	nrf.DynPayload	= false;
	nrf.Channel		= tc->Channel;
	//nrf.Channel		= 120;
	nrf.Speed		= tc->Speed;

	nrf.FixData	= Fix2401;

	if(WirelessLed) net->Led	= CreateFlushPort(WirelessLed);

	nrf.Master	= true;

	return &nrf;
}*/

void AP0803::Restore()
{
	if (!Client) return;

	if(Client) Client->Reset("按键重置");
}

int AP0803::GetStatus()
{
	if (!Client) return 0;

	return Client->Status;
}

void AP0803::OnLongPress(InputPort* port, bool down)
{
	if (down) return;

	debug_printf("Press P%c%d Time=%d ms\r\n", _PIN_NAME(port->_Pin), port->PressTime);

	ushort time = port->PressTime;
	auto client	= Client;
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

GPRS(COM4)
PE0					PWR_KEY
PD3					RST
PC10(TX4)		RXD	
PC11(RX4)		TXD	

PE9					KEY1
PE14				KEY2

PE5					LED1
PE4					LED2
PD0					LED3


USB
PA11 				_N
PA12				_P
*/
