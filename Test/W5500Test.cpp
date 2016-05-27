#include "Sys.h"
#include "Spi.h"
#include "Power.h"
#include "Net\ITransport.h"
#include "Socket.h"
#include "Message\DataStore.h"
#include "Drivers\W5500.h"

void TestTask(void* param)
{
    W5500* net = (W5500*)param;
	net->PhyStateShow();
	//static bool netStaChang = true;
	//if(netStaChang != net->CheckLink())
	//{
	//	net->PhyStateShow();
	//	netStaChang = !netStaChang;
	//}
}

void TestTask2(void* param)
{
	UdpClient* udp = (UdpClient*)param;
	debug_printf("尝试发送udp数据\r\n");
	byte buf[70] ;
	for(int i = 0; i < 70; i++)
		buf[i] = i;
	udp->Write(Buffer(buf,sizeof(buf)));
}

void SocketShow(void* param)
{
	UdpClient* udp = (UdpClient*)param;
	udp->StateShow();
}

void TestW5500(Spi* spi, Pin irq, Pin rst)
{
	auto net	= new W5500();
	net->Init(spi, irq, rst);

	net->IP = IPAddress(192, 168, 0, 200);
	net->Gateway = IPAddress(192,168,0,1);
	// 定时检查网络状况
	//Sys.AddTask(TestTask, net, 10000, 10000, "TestW5500");
	net->Open();

	net->StateShow();

	auto udp	= new UdpClient(*net);
	IPAddress RemoteIP(255, 255, 255, 255);

	udp->Remote.Address = RemoteIP;
	udp->Remote.Port = 65500;
	udp->StateShow();
	udp->Open();
	udp->StateShow();

	Sys.AddTask(TestTask2, udp, 9000, 10000, "TestUdpClient");
	Sys.Sleep(1000);
	String str	= "hello";
	udp->Write(str.GetBytes());

	//Sys.AddTask(SocketShow, udp, 9000, 10000, "show");
}
