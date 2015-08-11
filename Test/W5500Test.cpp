#include "Drivers\W5500.h"

void TestTask(void* param)
{
    W5500* net = (W5500*)param;
	if(net->CheckLink()) net->PhyStateShow();
}

void TestW5500(Spi* spi, Pin irq, OutputPort* reset)
{
	W5500* net = new W5500();
	net->Init(spi, irq, reset);

	net->IP = IPAddress(192, 168, 0, 200);

	if(net->CheckLink()) debug_printf("OK");
	net->StateShow();

	Sys.AddTask(TestTask, net, 10000000, 10000000, "TestW5500");
}
