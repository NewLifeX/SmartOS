#include "AP0802.h"

AP0802::AP0802(int hardver) : AP0801()
{
	LedPins.Clear();
	ButtonPins.Clear();

	if (hardver > 160712)
	{
		LedPins.Add(PE5);
		LedPins.Add(PE4);
		LedPins.Add(PD0);

		ButtonPins.Add(PE9);
		ButtonPins.Add(PE14);
	}
	else
	{
		LedPins.Add(PE5);
		LedPins.Add(PE4);
		LedPins.Add(PD0);

		ButtonPins.Add(PE13);
		ButtonPins.Add(PE14);
	}

	Esp.Power = PE0;
	Esp.Reset = PD3;
}

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
