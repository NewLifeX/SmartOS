#ifndef _Pandora_H_
#define _Pandora_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"

// 潘多拉0903
class PA0903
{
public:
	OutputPort		Leds[2];
	// InputPort		Buttons[2];

	OutputPort*		EthernetLed;	// 以太网指示灯
	OutputPort*		WirelessLed;	// 无线指示灯

	ISocketHost*	Host;			// 网络主机
	
	PA0903();

	void Setup(ushort code, const char* name, COM message = COM1, int baudRate = 0);

	ISocketHost* Create5500();
	//ITransport* Create2401();
	//ISocketHost* Create8266();

	TokenClient* CreateClient();

	void InitDHCP(Action onNetReady = nullptr);
	bool QueryDNS(TokenConfig& tk);
};

#endif
