#ifndef _AP0801_H_
#define _AP0801_H_

#include "Kernel\Sys.h"
#include "Net\ITransport.h"
#include "Net\Socket.h"

#include "App\Alarm.h"

#include "Device\Spi.h"
#include "Drivers\W5500.h"
#include "Drivers\Esp8266\Esp8266.h"

// 阿波罗0801
class AP0801
{
public:
	List<Pin>	LedPins;
	List<Pin>	ButtonPins;
	List<OutputPort*>	Leds;
	List<InputPort*>	Buttons;

	List<OutputPort*>	Outputs;
	List<InputPort*>	Inputs;

	Alarm*			AlarmObj;

	W5500Config		Net;
	Esp8266Config	Esp;

	uint	HardVer;

	AP0801();

	// 设置系统参数
	void Init(ushort code, cstring name, COM message = COM1);

	// 设置数据区
	void* InitData(void* data, int size);
	// 写入数据区并上报
	void Write(uint offset, byte data);
	//获取客户端的状态0，未握手，1已握手，2已经登陆
	int GetStatus();

	typedef bool(*Handler)(uint offset, uint size, bool write);
	void Register(uint offset, uint size, Handler hook);
	void Register(uint offset, IDataPort& dp);

	void InitLeds();
	void InitButtons(const Delegate2<InputPort&, bool>& press);

	// 打开以太网W5500
	NetworkInterface* Create5500();
	// 打开Esp8266，作为主控或者纯AP
	NetworkInterface* Create8266();

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
};

#endif
