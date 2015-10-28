#include "Tiny.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\ShunCom.h"
#include "Net\Zigbee.h"
#include "TinyNet\TinyClient.h"

uint OnSerial(ITransport* transport, ByteArray& bs, void* param, void* param2)
{
	debug_printf("OnSerial len=%d \t", bs.Length());
	bs.Show(true);

	TinyClient* client = TinyClient::Current;
	if(client) client->Store.Write(1, bs);

	return 0;
}

void Setup(ushort code, const char* name, COM_Def message, int baudRate)
{
	Sys.Code = code;
	Sys.Name = (char*)name;

    // 初始化系统
    //Sys.Clock = 48000000;
    Sys.MessagePort = message; // 指定printf输出的串口
    Sys.Init();
    Sys.ShowInfo();

#if DEBUG
	// 打开串口输入便于调试数据操作，但是会影响性能
	SerialPort* sp = SerialPort::GetMessagePort();
	if(baudRate != 1024000)
	{
		sp->Close();
		sp->SetBaudRate(baudRate);
		sp->Register(OnSerial);
		sp->Open();
	}

	WatchDog::Start(20000);
#else
	WatchDog::Start();
#endif
}

ITransport* Create2401(SPI_TypeDef* spi_, Pin ce, Pin irq, Pin power, bool powerInvert)
{
	static Spi spi(spi_, 10000000, true);
	static NRF24L01 nrf;
	nrf.Init(&spi, ce, irq, power);
	nrf.Power.Invert = powerInvert;

	nrf.AutoAnswer	= false;
	nrf.PayloadWidth= 32;
	nrf.Channel		= TinyConfig::Current->Channel;
	nrf.Speed		= TinyConfig::Current->Speed;

	//if(!nrf.Check()) debug_printf("请检查NRF24L01线路\r\n");

	return &nrf;
}

ITransport* CreateShunCom(COM_Def index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg)
{
	static SerialPort sp(index, baudRate);
	static ShunCom zb;
	zb.Power.Init(power, TinyConfig::Current->HardVer < 0x08);
	zb.Sleep.Init(slp, true);
	zb.Config.Init(cfg, true);
	zb.Init(&sp, rst);

	return &zb;
}

TinyClient* CreateTinyClient(ITransport* port)
{
	static TinyController ctrl;
	ctrl.Port	= port;
	static TinyClient tc(&ctrl);
	tc.Cfg		= TinyConfig::Current;

	TinyClient::Current	= &tc;

	//ctrl.Mode	= 3;
	//ctrl.Open();

	return &tc;
}

void* InitConfig(void* data, uint size)
{
	// Flash最后一块作为配置区
	Config::Current	= &Config::CreateFlash();

	// 启动信息
	HotConfig* hot	= &HotConfig::Current();
	hot->Times++;

	data = hot->Next();
	if(hot->Times == 1)
	{
		memset(data, 0x00, size);
		((byte*)data)[0] = size;
	}

	// 默认出厂设置
	static TinyConfig tc;
	TinyConfig::Current = &tc;
	tc.LoadDefault();
	tc.Channel	= 120;
	tc.Speed	= 250;
	tc.HardVer	= 0x08;

	// 尝试加载配置区设置
	tc.Load();

	return data;
}

/*void NoUsed()
{
	Setup(1234, "");
	Create2401(SPI1, P0, P0);
	CreateShunCom(COM2, 38400, P0, P0, P0, P0);
	CreateTinyClient(NULL);
}*/
