#include "Tiny.h"

#include "SerialPort.h"
#include "WatchDog.h"
#include "Config.h"

#include "Drivers\NRF24L01.h"
#include "Drivers\ShunCom.h"
#include "Net\Zigbee.h"
#include "TinyNet\TinyClient.h"

#define ShunCom_SLAVE 0
//#define MSG_DEBUG 0

uint OnSerial(ITransport* transport, Buffer& bs, void* param, void* param2)
{
	debug_printf("OnSerial len=%d \t", bs.Length());
	bs.Show(true);

	auto client = TinyClient::Current;
	if(client) client->Store.Write(1, bs);

	return 0;
}

void Setup(ushort code, cstring name, COM message, int baudRate)
{
	auto& sys	= (TSys&)Sys;
	sys.Code = code;
	sys.Name = (char*)name;

    // 初始化系统
    //Sys.Clock = 48000000;
    sys.Init();
#if DEBUG
    sys.MessagePort = message; // 指定printf输出的串口
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

#endif

	// Flash最后一块作为配置区
	Config::Current	= &Config::CreateFlash();
}

int Fix2401(const Buffer& bs)
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

ITransport* Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led)
{
	auto spi = new Spi(spi_, 10000000, true);
	auto nrf = new NRF24L01();
	nrf->Init(spi, ce, irq, power);

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

	nrf->AutoAnswer		= false;
	nrf->DynPayload		= false;
	nrf->Channel		= tc->Channel;
	//nrf->Channel		= 120;
	nrf->Speed			= tc->Speed;

	nrf->FixData	= Fix2401;
	nrf->Led		= led;

	byte num = tc->Mac[0] && tc->Mac[1] && tc->Mac[2] && tc->Mac[3] && tc->Mac[4];
	if(num != 0 && num != 0xFF) Buffer(nrf->Remote, 5)	= tc->Mac;

	return nrf;
}

void ShunComExternalCfg(void * param)
{
	auto zb = (ShunCom *)param;
	auto tc = TinyConfig::Current;

	if(tc->Channel != 0x0F)
	{
		if(zb->EnterConfig())
		{
			//zb->ShowConfig();
			zb->SetDevice(0x02);
			zb->SetSend(0x00);
			//zb->SetPanID(0x4444);
			//zb->EraConfig();
			//tc->Channel = 0x0F;
			tc->Save();
			zb->ExitConfig();
		}
	}
}

ITransport* CreateShunCom(COM index, int baudRate, Pin rst, Pin power, Pin slp, Pin cfg, IDataPort* led)
{
	auto tc	= TinyConfig::Create();
	debug_printf("tc->Interval %d\r\n",tc->Interval );
	if(tc->Channel == 0)
	{
		tc->Channel	= 0x0F;
		tc->Speed	= 250;
	}
	if(tc->Interval == 0)
	{
		tc->Interval= 800;
		tc->Timeout	= 2400;
	}
	auto sp = new SerialPort(index, baudRate);
	auto zb = new ShunCom();

	sp->Rx.SetCapacity(512);

	zb->Power.Set(power);
	zb->Sleep.Init(slp, true);
	zb->Config.Init(cfg, true);
	zb->Init(sp, rst);
#if ShunCom_SLAVE
	zb->ExternalCfg = ShunComExternalCfg;
#endif
	//zb->Open();

	//Sys.Sleep(300);
	//
	zb->Led	= led;
	return zb;
}

TinyClient* CreateTinyClient(ITransport* port)
{
	static TinyController ctrl;
	ctrl.Port	= port;
	ctrl.ApplyConfig();

	// 新配置需要保存一下
	auto tc = TinyConfig::Current;
	if(tc && tc->New) tc->Save();

	static TinyClient client(&ctrl);
	client.Cfg = TinyConfig::Current;

	TinyClient::Current	= &client;

	return &client;
}

void* InitConfig(void* data, uint size)
{
	// 启动信息
	auto hot	= &HotConfig::Current();
	hot->Times++;

	data = hot->Next();
	if(hot->Times == 1)
	{
		Buffer ds(data, size);
		ds.Clear();
		ds[0]	= size;
	}

	auto tc = TinyConfig::Create();

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
	Sys.Sleep(300);

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

void CheckUserPress3(void* sender)
{
	auto but = (Button_GrayLevel *)sender;
	CheckUserPress(&but->Key, but->Key.Read(), nullptr);
}

void InitButtonPress(Button_GrayLevel* btns, byte count)
{
	for(int i=0; i<count; i++)
	{
		//btns[i].Register(CheckUserPress3, nullptr);// = CheckUserPress2;
		btns[i].Press	= CheckUserPress3;
	}
}

void SetPower(ITransport* port)
{
	auto pwr	= dynamic_cast<Power*>(port);
	if(pwr) pwr->SetPower();
}
