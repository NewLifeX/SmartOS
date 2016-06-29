#ifndef _AP0801_H_
#define _AP0801_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"

// 阿波罗0801/0802
class AP0801
{
public:
	Pin		Leds[2];
	Pin		Buttons[2];

	Pin		EthernetLed;	// 以太网指示灯
	Pin		WirelessLed;	// 无线指示灯

	ISocketHost*	Host;	// 网络主机
	ISocketHost*	HostAP;	// 网络主机
	TokenClient*	Client;	// 令牌客户端

	AP0801();

	// 设置系统参数
	void Init(ushort code, cstring name, COM message = COM1, int baudRate = 0);

	// 设置数据区
	void* InitData(void* data, int size);
	void Register(int index, IDataPort* dps);

	// 打开以太网W5500
	ISocketHost* Create5500();

	// 打开Esp8266，作为主控或者纯AP
	ISocketHost* Create8266(bool apOnly);

	ITransport* Create2401();

	void InitClient();
	void InitNet();

private:
	void*	Data;
	int		Size;

	void OpenClient(ISocketHost& host);
	ISocket* AddControl(ISocketHost& host, const NetUri& uri);
};

#endif
