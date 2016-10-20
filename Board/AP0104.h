#ifndef _AP0104_H_
#define _AP0104_H_

#include "Sys.h"
#include "Net\ITransport.h"
#include "TokenNet\TokenClient.h"
#include "TinyNet\TinyServer.h"
#include "TokenNet\GateWay.h"

//#include "Message\ProxyFactory.h"
#include "App\Alarm.h"
#include "Device\RTC.h"

// 阿波罗0801/0802
class AP0104
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

	ITransport*		Nrf;	// NRF24L01传输口
	TinyServer*		Server; // TinyServer服务

	Gateway*		_GateWay;	// 网关

	// ProxyFactory*	ProxyFac;	// 透传管理器
	// Alarm*			AlarmObj;

	AP0104();	
	// 设置系统参数
	void Init(ushort code, cstring name, COM message = COM1);

	// 设置数据区
	void* InitData(void* data, int size);
	void Register(int index, IDataPort& dp);

	void InitLeds();
	void InitButtons(const Delegate2<InputPort&, bool>& press);

	// 打开以太网W5500
	ISocketHost* Create5500();
	// 打开Esp8266，作为主控或者纯AP
	ISocketHost* Create8266(bool apOnly);

	void InitClient();
	void InitNet();

	// 打开NRF24L01
	ITransport* Create2401();
	void	InitTinyServer();

	void CreateGateway();

	void Restore();
	static void OnPress(InputPort* port, bool down);
	static void OnLongPress(InputPort* port, bool down);

	static AP0104* Current;

private:
	void*	Data;
	int		Size;

	// Control 打开情况标识
	bool	NetMaster;
	bool	NetBra;
	bool	EspMaster;
	bool	EspBra;

	void OpenClient(ISocketHost& host);
	TokenController* AddControl(ISocketHost& host, const NetUri& uri, ushort localPort);

	static int Fix2401(const Buffer& bs);
};

#endif
