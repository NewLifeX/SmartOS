#ifndef _AP0104_H_
#define _AP0104_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"

// 阿波罗0801/0802
class AP0104
{
public:
	OutputPort		Leds[3];
	InputPort		Buttons[2];

	OutputPort*		EthernetLed;	// 以太网指示灯
	OutputPort*		WirelessLed;	// 无线指示灯

	ISocketHost*	Host;			// 网络主机
	
	AP0104();

	void Setup(ushort code, cstring name, COM message = COM1, int baudRate = 0);

	ISocketHost* Create5500();
	ITransport* Create2401();
	ISocketHost* Create8266(Action onNetReady = nullptr);

	TokenClient* CreateClient();

	void InitDHCP(Action onNetReady = nullptr);
	bool QueryDNS(TokenConfig& tk);
};

#endif
