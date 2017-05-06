#ifndef _Pandora_H_
#define _Pandora_H_

#include "Kernel\Sys.h"
#include "Net\ITransport.h"
#include "Net\Socket.h"

#include "Device\RTC.h"
#include "Message\ProxyFactory.h"
#include "App\Alarm.h"

// 潘多拉0903
class PA0903
{
public:
	List<Pin>	LedPins;
	List<Pin>	ButtonPins;
	List<OutputPort*>	Leds;
	List<InputPort*>	Buttons;

	//List<OutputPort*>	Outputs;
	//List<InputPort*>	Inputs;

	ProxyFactory*	ProxyFac;	// 透传管理器
	Alarm*			AlarmObj;
	
	PA0903();

	// 设置数据区
	void* InitData(void* data, int size);
	// 写入数据区并上报
	void Write(uint offset, byte data);
	void Register(int index, IDataPort& dp);

	void InitLeds();
	void InitButtons(const Delegate2<InputPort&, bool>& press);

	// 打开以太网W5500
	NetworkInterface* Create5500();

	// 打开Esp8266，作为主控或者纯AP
	NetworkInterface* Create8266();
	//ITransport* Create2401();

	void InitClient();
	void InitNet();
	void InitProxy();
	void InitAlarm();

	void Restore();
	void OnLongPress(InputPort* port, bool down);

	static PA0903* Current;

private:
	void*	Data;
	int		Size;
};

#endif
