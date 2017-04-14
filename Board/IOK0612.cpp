#include "IOK0612.h"

IOK0612* IOK0612::Current = nullptr;

IOK0612::IOK0612()
{
	LedPins.Clear();
	ButtonPins.Clear();
	LedPins.Add(PB7);
	ButtonPins.Add(PB6);

	Esp.Com = COM2;
	Esp.Baudrate = 115200;
	Esp.Power = PB12;
	Esp.Reset = PA1;
	Esp.LowPower = P0;

	Current = this;
}

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
