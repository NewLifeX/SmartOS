#include "Drivers\W5500.h"

void TestTask(void* param)
{
    W5500* net = (W5500*)param;
	if(net->CheckLnk()) net->PhyStateShow();
}

void TestW5500(Spi* spi, Pin irq, OutputPort* reset)
{
	W5500* _Net = new W5500();
	_Net->Init(spi,irq,reset);

	Sys.Sleep(200);
	MacAddress mac = 0x000066554433221100;
	_Net->SetMac(mac);
	//_Net->AutoMac();
	_Net->DefGateway();
	_Net->DefIpMask();

	IPAddress myip(192, 168, 0, 200);
	_Net->SetMyIp(myip);
	_Net->OpenPingACK();
	_Net->StateShow();

	if(_Net->CheckLnk()) debug_printf("OK");

	Sys.AddTask(TestTask, _Net, 10000000, 10000000, "TestW5500");
}
