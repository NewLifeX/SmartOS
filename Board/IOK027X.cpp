#include "IOK027X.h"

#include "Drivers\Esp8266\Esp8266.h"

IOK027X* IOK027X::Current = nullptr;

IOK027X::IOK027X()
{
	LedPins.Clear();
	ButtonPins.Clear();
	LedPins.Add(PA4);
	LedPins.Add(PA5);

	Esp.Com = COM2;
	Esp.Baudrate = 115200;
	Esp.Power = PB2;
	Esp.Reset = PA1;
	Esp.LowPower = P0;

	LedsShow = 2;
	LedInvert = true;
	LedsTaskId = 0;

	Current = this;
}

// 联动触发 
static void UnionPress(InputPort& port, bool down)
{
	if (IOK027X::Current == nullptr) return;
	auto client = IOK027X::Current->Client;

	byte data[1];
	data[0] = down ? 0 : 1;

	client->Store.Write(port.Index + 1, Buffer(data, 1));
	// 主动上报状态
	client->ReportAsync(port.Index + 1, 1);

}

static bool ledstat2 = false;
//重写重置方法，使得重置时红灯闪烁
void IOK027X::Restore()
{
	if (!Client) return;
	for (int i = 0; i < 10; i++)
	{
		Leds[1]->Write(ledstat2);
		ledstat2 = !ledstat2;
		Sys.Sleep(300);
	}

	if (Client) Client->Reset("按键重置");
}

void IOK027X::Union(Pin pin1, Pin pin2, bool invert)
{
	Pin p[] = { pin1,pin2 };
	for (int i = 0; i < 2; i++)
	{
		if (p[i] == P0) continue;

		auto port = new InputPort(p[i]);
		port->Invert = invert;	// 是否倒置输入输出
		//port->ShakeTime = shake;
		port->Index = i;
		port->Press.Bind(UnionPress);
		port->UsePress();
		port->Open();
	}
}
//重写指示灯默认倒置
void IOK027X::InitLeds()
{
	for (int i = 0; i < LedPins.Count(); i++)
	{
		auto port = new OutputPort(LedPins[i], LedInvert);
		//auto port = new OutputPort(LedPins[i], false);
		port->Open();
		port->Write(false);
		Leds.Add(port);
	}
}


/*void IOK027X::FlushLed()
{
	if (LedsShow == 0)			// 启动时候20秒
	{
		auto esp = dynamic_cast<Esp8266*>(Host);
		if (esp && esp->Linked)					// 8266 初始化完成  且  连接完成
		{
			Sys.SetTaskPeriod(LedsTaskId, 500);	// 慢闪
		}

		Leds[0]->Write(ledstat2);
		ledstat2 = !ledstat2;

		if (Sys.Ms() > 20000)
		{
			Leds[0]->Write(false);
			LedsShow = 2;	// 置为无效状态
		}
	}

	bool stat = false;
	// 3分钟内 Client还活着则表示  联网OK
	if (Client && Client->LastActive + 180000 > Sys.Ms() && LedsShow == 1)stat = true;
	Leds[1]->Write(stat);
	if (LedsShow == 2)Sys.SetTask(LedsTaskId, false);
}

byte IOK027X::LedStat(byte showmode)
{
	auto esp = dynamic_cast<Esp8266*>(Host);
	if (esp)
	{
		if (showmode == 1)
		{
			esp->RemoveLed();
			esp->SetLed(*Leds[0]);
		}
		else
		{
			esp->RemoveLed();
			// 维护状态为false
			Leds[0]->Write(false);
		}
	}

	if (showmode != 2)
	{
		if (!LedsTaskId)
		{
			LedsTaskId = Sys.AddTask(&IOK027X::FlushLed, this, 500, 100, "CltLedStat");
			debug_printf("AddTask(IOK027X:FlushLed)\r\n");
		}
		else
		{
			Sys.SetTask(LedsTaskId, true);
			if (showmode == 1)Sys.SetTaskPeriod(LedsTaskId, 500);
		}
		LedsShow = showmode;
	}
	if (showmode == 2)
	{
		// 由任务自己结束，顺带维护输出状态为false
		// if (LedsTaskId)Sys.SetTask(LedsTaskId, false);
		LedsShow = 2;

	}
	return LedsShow;
}*/

/*
NRF24L01+ 	(SPI3)		|	W5500		(SPI2)		|	TOUCH		(SPI3)
NSS			|				NSS			|	PD6			NSS
CLK			|				SCK			|				SCK
MISO		|				MISO		|				MISO
MOSI		|				MOSI		|				MOSI
PE3			IRQ			|	PE1			INT(IRQ)	|	PD11		INT(IRQ)
PD12		CE			|	PD13		NET_NRST	|				NET_NRST
PE6			POWER		|				POWER		|				POWER

ESP8266		(COM4)
TX
RX
PD3			RST
PE2			POWER

TFT
FSMC_D 0..15		TFT_D 0..15
NOE					RD
NWE					RW
NE1					RS
PE4					CE
PC7					LIGHT
PE5					RST

PE13				KEY1
PE14				KEY2

PE15				LED2
PD8					LED1


USB
PA11 PA12
*/
