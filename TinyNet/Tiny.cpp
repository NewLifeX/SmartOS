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

	auto client = TinyClient::Current;
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
	if(baudRate > 0)
	{
		auto sp = SerialPort::GetMessagePort();
		if(baudRate != 1024000)
		{
			sp->Close();
			sp->SetBaudRate(baudRate);
		}
		sp->Register(OnSerial);
	}

	//WatchDog::Start(20000);
#else
	WatchDog::Start();
#endif
}

void Fix2401(void* param)
{
	auto& bs	= *(Array*)param;
	// 微网指令特殊处理长度
	uint rs	= bs.Length();
	if(rs >= 8)
	{
		rs = bs[5] + 8;
		if(rs < bs.Length()) bs.SetLength(rs);
	}
}

ITransport* Create2401(SPI_TypeDef* spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
{
	auto spi = new Spi(spi_, 10000000, true);
	auto nrf = new NRF24L01();
	nrf->Init(spi, ce, irq, power);

	auto tc	= TinyConfig::Current;
	nrf->AutoAnswer		= true;
	nrf->DynPayload		= true;
	//nrf->Channel		= tc->Channel;
	nrf->Channel		= 120;
	nrf->Speed			= tc->Speed;

	nrf->FixData	= Fix2401;
	nrf->Led		= led;

	byte num = tc->Mac[0] && tc->Mac[1] && tc->Mac[2] && tc->Mac[3] && tc->Mac[4];
	if(num != 0 && num != 0xFF) memcpy(nrf->Remote, tc->Mac, 5);

	return nrf;
}

ITransport* CreateShunCom(COM_Def index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg, IDataPort* led)
{
	auto sp = new SerialPort(index, baudRate);
	auto zb = new ShunCom();

	sp->Rx.SetCapacity(512);

	zb->Power.Set(power);
	zb->Sleep.Init(slp, true);
	zb->Config.Init(cfg, true);
	zb->Init(sp, rst);
	if(zb->EnterConfig())
	{			
		zb->ShowConfig();
		zb->SetDevice(0x02);
		zb->SetSend(0x01);
		//zb->SetPanID(0x6766);
		//zb->EraConfig();
		zb->ExitConfig();
	}	
	zb->Led	= led;

	return zb;
}

TinyClient* CreateTinyClient(ITransport* port)
{
	static TinyController ctrl;
	ctrl.Port	= port;

	// 只有2401需要打开重发机制
	if(strcmp(port->ToString(), "R24") != 0) ctrl.Timeout = -1;

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
	auto hot	= &HotConfig::Current();
	hot->Times++;

	data = hot->Next();
	if(hot->Times == 1)
	{
		memset(data, 0x00, size);
		((byte*)data)[0] = size;
	}

	auto tc = TinyConfig::Init();

	// 尝试加载配置区设置
	tc->Load();

	return data;
}

void ClearConfig()
{		
	auto tc = TinyConfig::Current;
	if(tc) tc->Clear();

	// 退网
	auto client = TinyClient::Current;
	if(client) client->DisJoin();

	Sys.Reset();
}

bool CheckUserPress(InputPort* port, bool down, void* param)
{
	if(down || port->PressTime < 1000) return false;

	debug_printf("按下 P%c%d 时间=%d 毫秒 \r\n", _PIN_NAME(port->_Pin), port->PressTime);

	// 按下5秒，清空设置并重启
	if(port->PressTime >= 5000)
	{
		ClearConfig();

		return true;
	}
	// 按下3秒，重启
	else if(port->PressTime >= 3000)
	{
		Sys.Reset();

		return true;
	}

	return false;
}

void CheckUserPress2(InputPort* port, bool down, void* param)
{
	CheckUserPress(port, down, param);
}

void InitButtonPress(Button_GrayLevel* btns, byte count)
{
	for(int i=0; i<count; i++)
	{
		btns[i].OnPress	= CheckUserPress2;
	}
}

void SetPower(ITransport* port)
{
	auto pwr	= dynamic_cast<Power*>(port);
	if(pwr) pwr->SetPower();
}
