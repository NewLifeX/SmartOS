#include "AP0802.h"

AP0802::AP0802(int hardver) : AP0801()
{
	LedPins.Clear();
	ButtonPins.Clear();

	LedPins.Add(PE5);
	LedPins.Add(PE4);
	LedPins.Add(PD0);

	Sys.HardVer = hardver;
	if (hardver >= 160712)
		ButtonPins.Add(PE9);
	else
		ButtonPins.Add(PE13);

	ButtonPins.Add(PE14);

	if (hardver >= 170106)
		Esp.Power = PE2;
	else
		Esp.Power = PE0;
	Esp.Reset = PD3;
}

/*void AP0802::InitLeds()
{
	for (int i = 0; i < LedPins.Count(); i++)
	{
		// 旧版本没有加上拉下拉电阻
		auto port = new OutputPort(LedPins[i], HardVer <= 170106 ? 0 : 2);
		//if(HardVer <= 170309) port->Invert = false;
		port->Open();
		*port	= false;
		Leds.Add(port);
	}
}*/

/*
NRF24L01+ 	(SPI3)		|	W5500		(SPI2)
NSS						|				NSS
CLK						|				SCK
MISO					|				MISO
MOSI					|				MOSI
PE3			IRQ			|	PE1			INT(IRQ)
PD12		CE			|	PD13		NET_NRST
PE6			POWER		|				POWER

ESP8266		(COM4)
TX
RX
PD3			RST
PE2			POWER

				KEY1
				KEY2

				LED1
				LED2
				LED3

USB
PA11 PA12
*/
