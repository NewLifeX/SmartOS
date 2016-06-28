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

	OutputPort*		EthernetLed;	// 以太网指示灯
	OutputPort*		WirelessLed;	// 无线指示灯

	ISocketHost*	Host;	// 网络主机
	ISocketHost*	HostAP;	// 网络主机
	TokenClient*	Client;	// 令牌客户端
	
	PA0903();

	void Setup(ushort code, cstring name, COM message = COM1, int baudRate = 0);
	
	// 打开以太网W5500，如果网络未接通，则返回空
	ISocketHost* Open5500();
	static ISocketHost* Create5500(SPI spi, Pin irq, Pin rst = P0, IDataPort* led = nullptr);
	
	void CreateClient();
	void OpenClient();
	void AddControl(ISocketHost& host, TokenConfig& cfg);

};

#endif
