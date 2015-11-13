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
	nrf->SetPower();

	nrf->AutoAnswer		= false;
	nrf->PayloadWidth	= 32;
	nrf->Channel		= TinyConfig::Current->Channel;
	nrf->Speed			= TinyConfig::Current->Speed;

	//if(!nrf.Check()) debug_printf("请检查NRF24L01线路\r\n");

	return nrf;
}

uint OnZig(ITransport* port, Array& bs, void* param, void* param2)
{
	debug_printf("配置信息\r\n");
	bs.Show(true);

	return 0;
}

ITransport* CreateShunCom(COM_Def index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg)
{
	SerialPort* sp = new SerialPort(index, baudRate);
	ShunCom* zb = new ShunCom();
	//zb.Power.Init(power, TinyConfig::Current->HardVer < 0x08);
	//InputPort temp;
	//temp.Set(power).Open();
	//bool dd = temp;
	//temp.Close();
	
	zb->Power.Set(power).Open();
	
	if(zb->Power) zb->Power.Invert = true;
	//if(dd) zb->Power.Invert = true;

	zb->Sleep.Init(slp, true);
	zb->Config.Init(cfg, true);
	zb->Init(sp, rst);

	sp->SetPower();
	zb->SetPower();

	/*zb.Register(OnZig, &zb);
	zb.Open();

	zb.Config	= true;
	Sys.Sleep(1200);

	debug_printf("进入配置模式\r\n");

	byte buf[] = {0xFE, 0x00, 0x21, 0x15, 0x34};
	zb.Write(CArray(buf));*/

	/*ByteArray bs;
	int n=10000;
	while(n--)
	{
		zb.Read(bs);
		if(bs.Length() > 0)
		{
			bs.Show(true);
			break;
		}
	}
	debug_printf("退出配置模式\r\n");
	zb.Config	= false;*/

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

/*void NoUsed()
{
	Setup(1234, "");
	Create2401(SPI1, P0, P0);
	CreateShunCom(COM2, 38400, P0, P0, P0, P0);
	CreateTinyClient(NULL);
}*/
