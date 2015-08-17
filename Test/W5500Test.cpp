#include "Drivers\W5500.h"

void TestTask(void* param)
{
    W5500* net = (W5500*)param;
	//if(net->CheckLink()) net->PhyStateShow();
	static bool netStaChang = true;
	if(netStaChang != net->CheckLink())
	{
		net->PhyStateShow();
		netStaChang = !netStaChang;
	}
}

void TestTask2(void* param)
{
	UdpClient* udp = (UdpClient*)param;
	debug_printf("尝试发送udp数据\r\n");
	byte buf[]  = "hello";
	udp->Write(buf,sizeof(buf));
}

void TestW5500(Spi* spi, Pin irq, OutputPort* reset)
{
	W5500* net = new W5500();
	net->Init(spi, irq, reset);

	net->IP = IPAddress(192, 168, 0, 200);
	net->Gateway = IPAddress(192,168,0,1);
	// 定时检查网络状况
	Sys.AddTask(TestTask, net, 10000000, 10000000, "TestW5500");
	net->StateShow();
	net->Open();
	
	UdpClient *udp = new UdpClient(net);
	IPAddress RemoteIP(192,168,0,16);
	udp->Remote = IPEndPoint(RemoteIP,3377);
	udp->Open();
	Sys.AddTask(TestTask2, udp, 9000000, 10000000, "TestUdpClient");
	
}
