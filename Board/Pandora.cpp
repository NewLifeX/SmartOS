#include "Pandora.h"

PA0903* PA0903::Current = nullptr;

PA0903::PA0903()
{
	LedPins.Add(PB1);
	LedPins.Add(PB14);

	Net.Spi = Spi1;
	Net.Irq = PA8;
	Net.Reset = PA0;

	Esp.Com = COM4;
	Esp.Baudrate = 115200;
	Esp.Power = PE2;
	Esp.Reset = PD3;
	Esp.LowPower = P0;

	Current = this;
}
