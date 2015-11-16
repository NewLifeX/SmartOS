#include "Tiny.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\ShunCom.h"
#include "Net\Zigbee.h"
#include "TinyNet\TinyClient.h"

uint OnSerial(ITransport* transport, Array& bs, void* param, void* param2)
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
    Sys.Init();
#if DEBUG
    Sys.MessagePort = message; // 指定printf输出的串口
    Sys.ShowInfo();
#endif

#if DEBUG
	// 打开串口输入便于调试数据操作，但是会影响性能
	SerialPort* sp = SerialPort::GetMessagePort();
	if(baudRate != 1024000)
	{
		sp->Close();
		sp->SetBaudRate(baudRate);
	}
	sp->Register(OnSerial);

	//WatchDog::Start(20000);
#else
	WatchDog::Start();
#endif
}

ITransport* Create2401(SPI_TypeDef* spi_, Pin ce, Pin irq, Pin power, bool powerInvert)
{
	Spi* spi = new Spi(spi_, 10000000, true);
	NRF24L01* nrf = new NRF24L01();
	nrf->Init(spi, ce, irq, power);
	nrf->Power.Invert = powerInvert;
	//nrf->SetPower();

	nrf->AutoAnswer		= false;
	nrf->PayloadWidth	= 32;
	nrf->Channel		= TinyConfig::Current->Channel;
	nrf->Speed			= TinyConfig::Current->Speed;

	//if(!nrf.Check()) debug_printf("请检查NRF24L01线路\r\n");

	return nrf;
}

ITransport* CreateShunCom(COM_Def index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg)
{
	SerialPort* sp = new SerialPort(index, baudRate);
	ShunCom* zb = new ShunCom();

	zb->Power.Set(power);
	if(zb->Power.ReadInput()) zb->Power.Invert = true;

	zb->Sleep.Init(slp, true);
	zb->Config.Init(cfg, true);
	zb->Init(sp, rst);

	//sp->SetPower();
	//zb->SetPower();

	return zb;
}

TinyClient* CreateTinyClient(ITransport* port)
{
	static TinyController ctrl;
	ctrl.Port	= port;

	// 只有2401需要打开重发机制
	if(strcmp(port->ToString(), "R24")) ctrl.Timeout = -1;

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

	TinyConfig* tc = TinyConfig::Init();

	// 尝试加载配置区设置
	tc->Load();

	return data;
}

void ClearConfig()
{
	TinyConfig* tc = TinyConfig::Current;
	if(tc) tc->Clear();

	// 退网
	TinyClient* client = TinyClient::Current;
	if(client) client->DisJoin();

	Sys.Reset();
}

void CheckUserPress(InputPort* port, bool down, void* param)
{
	if(down) return;

	debug_printf("按下 P%c%d 时间=%d 毫秒 \r\n", _PIN_NAME(port->_Pin), port->PressTime);

	// 按下5秒，清空设置并重启
	if(port->PressTime >= 5000)
		ClearConfig();
	// 按下3秒，重启
	else if(port->PressTime >= 3000)
		Sys.Reset();
}

void InitButtonPress(Button_GrayLevel* btns, byte count)
{
	for(int i=0; i<count; i++)
	{
		btns[i].OnPress	= CheckUserPress;
	}
}

void SetPower(ITransport* port)
{
	Power* pwr	= dynamic_cast<Power*>(port);
	if(pwr) pwr->SetPower();
}

