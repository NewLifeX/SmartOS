#include "IOK027X.h"

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
