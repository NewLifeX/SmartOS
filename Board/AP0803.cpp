#include "AP0803.h"

#include "Drivers\A67.h"

#include "Message\ProxyFactory.h"

AP0803* AP0803::Current = nullptr;
static ProxyFactory*	ProxyFac = nullptr;	// 透传管理器

AP0803::AP0803()
{
	LedPins.Add(PE5);
	LedPins.Add(PE4);
	LedPins.Add(PD0);
	ButtonPins.Add(PE9);
	ButtonPins.Add(PE14);

	Client = nullptr;
	ProxyFac = nullptr;
	AlarmObj = nullptr;

	Gsm.Com = COM4;
	Gsm.Baudrate = 115200;
	Gsm.Power = PE0;
	Gsm.Reset = PD3;
	Gsm.LowPower = P0;

	Current = this;
}

NetworkInterface* AP0803::CreateGPRS()
{
	debug_printf("\r\nCreateGPRS::Create \r\n");

	auto net = new GSM07();
	net->Init(Gsm.Com, Gsm.Baudrate);
	net->Set(Gsm.Power, Gsm.Reset, Gsm.LowPower);
	net->SetLed(*Leds[0]);

	net->DataKeys.Add("A6", "+CIPRCV:");
	net->DataKeys.Add("SIM900A", "\r\n+IPD,");

	if (!net->Open())
	{
		delete net;
		return nullptr;
	}

	return net;
}

static void OnInitNet(void* param)
{
	auto& bsp = *(AP0803*)param;

	bsp.CreateGPRS();

	bsp.Client->Open();
}

void AP0803::InitNet()
{
	Sys.AddTask(OnInitNet, this, 0, -1, "InitNet");
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
	auto bs = item.GetData();
	debug_printf("OnAlarm ");
	bs.Show(true);

	if (bs[1] == 0x06)
	{
		auto client = AP0803::Current->Client;
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

	AlarmObj->OnAlarm = OnAlarm;
	AlarmObj->Start();
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
