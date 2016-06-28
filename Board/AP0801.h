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

	ISocketHost*	Host;	// 网络主机
	ISocketHost*	HostAP;	// 网络主机
	TokenClient*	Client;	// 令牌客户端

	AP0801();

	// 设置系统参数
	void Setup(ushort code, cstring name, COM message = COM1, int baudRate = 0);
	
	// 设置数据区
	void* SetData(void* data, int size);
	void Register(int index, IDataPort* dps, int count);

	// 打开以太网W5500，如果网络未接通，则返回空
	ISocketHost* Open5500();
	static ISocketHost* Create5500(SPI spi, Pin irq, Pin rst = P0, IDataPort* led = nullptr);

	// 打开Esp8266，作为主控或者纯AP
	ISocketHost* Open8266(bool apOnly);
	static ISocketHost* Create8266(COM idx, Pin power, Pin rst);

	ITransport* Create2401();

	void CreateClient();
	void OpenClient();
	ISocket* AddControl(ISocketHost& host, const NetUri& uri);
	
private:
	void*	Data;
	int		Size;
};

#endif
