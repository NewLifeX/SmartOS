#include "Sys.h"
#include "W5500.h"




void TestTask(void* param)
{
    W5500* net = (W5500*)param;
	if(net->CheckLnk())
		net->PhyStateShow();
}

void TestW5500(Spi* spi, Pin irq, OutputPort* reset)
{
	W5500* _Net = new W5500();
	_Net->Init(spi,irq,reset);
	Sys.Sleep(200);
	MacAddress mac = 0x000066554433221100;
	_Net->StateShow();
	_Net->SetMac(mac);
	
	if(_Net->CheckLnk())
		debug_printf("OK");
	Sys.AddTask(TestTask,_Net,10000000,10000000);
}
