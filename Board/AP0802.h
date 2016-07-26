#ifndef _AP0802_H_
#define _AP0802_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"

// 阿波罗0801/0802
class AP0802
{
public:
	List<Pin>	LedPins;
	List<Pin>	ButtonPins;
	List<OutputPort*>	Leds;
	List<InputPort*>	Buttons;

	List<OutputPort*>	Outputs;
	List<InputPort*>	Inputs;

	ISocketHost*	Host;	// 网络主机
	ISocketHost*	HostAP;	// 网络主机
	TokenClient*	Client;	// 令牌客户端

	AP0802();

	// 设置系统参数
	void Init(ushort code, cstring name, COM message = COM1);

	// 设置数据区
	void* InitData(void* data, int size);
	void Register(int index, IDataPort& dp);

	void InitLeds();
	void InitButtons();
	void InitPort();

	// 打开以太网W5500
	ISocketHost* Create5500();

	// 打开Esp8266，作为主控或者纯AP
	ISocketHost* Create8266(bool apOnly);

	ITransport* Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led);
	ITransport* Create2401();

	void InitClient();
	void InitNet();

	void Restore();
	void OnLongPress(InputPort* port, bool down);

private:
	void*	Data;
	int		Size;

	void OpenClient(ISocketHost& host);
	TokenController* AddControl(ISocketHost& host, const NetUri& uri, ushort localPort);
};

#endif
