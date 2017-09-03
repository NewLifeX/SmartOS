#include "W5500Module.h"

#include "Drivers\W5500.h"

W5500Module::W5500Module()
{
	Net.Spi = Spi2;
	Net.Irq = PE1;
	Net.Reset = PD13;
}

NetworkInterface* W5500Module::Create5500(OutputPort* led)
{
	debug_printf("\r\nW5500::Create \r\n");

	auto net = new W5500(Net.Spi, Net.Irq, Net.Reset);
	if (!net->Open())
	{
		delete net;
		return nullptr;
	}

	if (led) net->SetLed(*led);
	net->EnableDNS();
	net->EnableDHCP();

	return net;
}
