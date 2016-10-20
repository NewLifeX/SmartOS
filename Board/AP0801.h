﻿#ifndef _AP0801_H_
#define _AP0801_H_

#include "Sys.h"
#include "Net\ITransport.h"
#include "Net\Socket.h"

//#include "TokenNet\TokenClient.h"
//#include "Message\ProxyFactory.h"
#include "App\Alarm.h"

// 阿波罗0801/0802
class AP0801
{
public:
	List<Pin>	LedPins;
	List<Pin>	ButtonPins;
	List<OutputPort*>	Leds;
	List<InputPort*>	Buttons;

	List<OutputPort*>	Outputs;
	List<InputPort*>	Inputs;

	// HardRTC*		Rtc;

	ISocketHost*	Host;	// 网络主机
	ISocketHost*	HostAP;	// 网络主机
	//TokenClient*	Client;	// 令牌客户端
	//ProxyFactory*	ProxyFac;	// 透传管理器
	Alarm*			AlarmObj;

	AP0801();

	// 设置系统参数
	void Init(ushort code, cstring name, COM message = COM1);

	// 设置数据区
	void* InitData(void* data, int size);
	// 设置TokenClient数据区
	void SetStore(void*data, int len);
	//获取客户端的状态0，未握手，1已握手，2已经登陆
	int GetStatus();
	
	typedef bool(*Handler)(uint offset, uint size, bool write);
	void Register(uint offset, uint size, Handler hook);
	void Register(uint offset, IDataPort& dp);

	void InitLeds();
	void InitButtons(InputPort::IOReadHandler press = nullptr);
	// void InitPort();

	// 打开以太网W5500
	ISocketHost* Create5500();
	// 打开Esp8266，作为主控或者纯AP
	ISocketHost* Create8266(bool apOnly);

	// ITransport* Create2401();

	void InitClient();
	void InitNet();
	void InitProxy();
	void InitAlarm();
	void Restore();
	// invoke指令
	void Invoke(const String& ation, const Buffer& bs);
	void OnLongPress(InputPort* port, bool down);

	static AP0801* Current;

private:
	void*	Data;
	int		Size;
	// Control 打开情况标识
	bool	NetMaster;
	bool	NetBra;
	bool	EspMaster;
	bool	EspBra;

	// uint	Flag;		// 内部使用  标识Controller创建情况

	void OpenClient(ISocketHost& host);
	void AddControl(ISocketHost& host, const NetUri& uri, ushort localPort);
};

#endif
