#include "IOK0612.h"
#include "Drivers\Esp8266\Esp8266.h"

IOK0612* IOK0612::Current = nullptr;

IOK0612::IOK0612()
{
	LedPins.Clear();
	ButtonPins.Clear();
	LedPins.Add(PB10);
	ButtonPins.Add(PB6);

	Esp.Com = COM2;
	Esp.Baudrate = 115200;
	Esp.Power = PB12;
	Esp.Reset = PA1;
	Esp.LowPower = P0;

	Current = this;
}
//static bool ledstat2 = false;

//重写指示灯默认倒置
void IOK0612::InitLeds()
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


/*void IOK0612::FlushLed()
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

byte IOK0612::LedStat(byte showmode)
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
			LedsTaskId = Sys.AddTask(&IOK0612::FlushLed, this, 500, 100, "CltLedStat");
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
NRF24L01+ 	(SPI3)
NSS			|
CLK			|
MISO		|
MOSI		|
PE3			IRQ
PD12		CE
PE6			POWER

ESP8266		(COM4)
TX
RX
PD3			RST
PE2			POWER

LED1
LED2

KEY1
KEY2

*/
