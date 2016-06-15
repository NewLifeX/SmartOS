#ifndef _AP0801_H_
#define _AP0801_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"

// 阿波罗0801/0802
class AP0801
{
public:
	OutputPort		Leds[2];
	InputPort		Buttons[2];

	OutputPort*		EthernetLed;	// 以太网指示灯
	OutputPort*		WirelessLed;	// 无线指示灯

	ISocketHost*	Host;			// 网络主机
	
	TokenClient*	Client;
	
	AP0801();

	void Setup(ushort code, cstring name, COM message = COM1, int baudRate = 0);

	ISocketHost* Create5500();
	static ISocketHost* Create5500(SPI spi, Pin irq, Pin rst = P0, IDataPort* led = nullptr);
	ITransport* Create2401();
	ISocketHost* Create8266();

	TokenClient* CreateClient();

	void InitDHCP(Action onNetReady = nullptr);
};

#endif
