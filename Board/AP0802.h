#ifndef _AP0802_H_
#define _AP0802_H_

#include "Kernel\Sys.h"
#include "Net\ITransport.h"
#include "Net\Socket.h"

#include "TokenNet\TokenClient.h"
#include "Device\Port.h"
#include "App\Alarm.h"

#define HardwareVerFist		0
#define HardwareVerAt160712 1
#define HardwareVerLast		2

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

	byte HardwareVer;

	AP0802();

	static AP0802* Current;
	// 设置系统参数
	void Init(ushort code, cstring name, COM message = COM1);

	// 设置数据区
	void* InitData(void* data, int size);
	void Register(int index, IDataPort& dp);
	void InitAlarm();
	void InitLeds();
	void InitButtons(const Delegate2<InputPort&, bool>& press);
	void InitPort();

	// 打开以太网W5500
	NetworkInterface* Create5500();

	// 打开Esp8266，作为主控或者纯AP
	NetworkInterface* Create8266(bool apOnly);

	ITransport* Create2401(SPI spi_, Pin ce, Pin irq, Pin power, bool powerInvert, IDataPort* led);
	ITransport* Create2401();

	void InitClient();
	void InitNet();

	void Restore();
	static void OnLongPress(InputPort* port, bool down);

private:
	void*	Data;
	int		Size;
	Alarm*	AlarmObj;

};

#endif
