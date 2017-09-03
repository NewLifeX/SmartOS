#include "GsmModule.h"

#include "Drivers\A67.h"
#include "Drivers\Sim900A.h"

GsmModule::GsmModule()
{
	Gsm.Com = COM4;
	Gsm.Baudrate = 115200;
	Gsm.Power = PE0;		// 0A04（170509）板上电源为PA4
	Gsm.Reset = PD3;
	Gsm.LowPower = P0;
}

static NetworkInterface* CreateGPRS(GSM07* net, const SerialConfig& gsm, OutputPort* led)
{
	net->Init(gsm.Com, gsm.Baudrate);
	net->Set(gsm.Power, gsm.Reset, gsm.LowPower);
	if (led) net->SetLed(*led);

	if (!net->Open())
	{
		delete net;
		return nullptr;
	}

	return net;
}

NetworkInterface* GsmModule::CreateA67(OutputPort* led)
{
	debug_printf("\r\nCreateA67::Create \r\n");

	auto net = new A67();

	return CreateGPRS(net, Gsm, led);
}

NetworkInterface* GsmModule::CreateSIM900A(OutputPort* led)
{
	debug_printf("\r\nCreateSIM900A::Create \r\n");

	auto net = new Sim900A();

	return CreateGPRS(net, Gsm, led);
}
