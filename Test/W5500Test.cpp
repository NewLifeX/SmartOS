#include "Drivers\W5500.h"

void TestTask(void* param)
{
    W5500* net = (W5500*)param;
	if(net->CheckLink()) net->PhyStateShow();
}

void TestW5500(Spi* spi, Pin irq, OutputPort* reset)
{
	W5500* _Net = new W5500();
	_Net->Init(spi, irq, reset);

	//Sys.Sleep(200);
	//_Net->Mac = 0x000066554433221100;
	//_Net->AutoMac();
	//_Net->DefGateway();
	//_Net->DefIpMask();

	_Net->IP = IPAddress(192, 168, 0, 200);
	//_Net->SetMyIp(myip);
	//_Net->OpenPingACK();

	if(_Net->CheckLink()) debug_printf("OK");
	_Net->StateShow();

	Sys.AddTask(TestTask, _Net, 10000000, 10000000, "TestW5500");
}
