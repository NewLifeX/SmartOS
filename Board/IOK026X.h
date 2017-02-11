#ifndef _IOK026X_H_
#define _IOK026X_H_

#include "Sys.h"
#include "Net\ITransport.h"
#include "Net\Socket.h"

#include "TokenNet\TokenClient.h"
#include "App\Alarm.h"
#include "Device\RTC.h"

// WIFI触摸开关 123位
class IOK026X
{
public:
	List<Pin>	LedPins;
	List<OutputPort*>	Leds;

	byte LedsShow;					// LED 显示状态开关  0 刚启动时候的20秒   1 使能   2 失能

	NetworkInterface*	Host;			// 网络主机
	TokenClient*	Client;			//
	Alarm*			AlarmObj;
	uint			LedsTaskId;

	IOK026X();

	void Init(ushort code, cstring name, COM message = COM1);

	void* InitData(void* data, int size);
	void Register(int index, IDataPort& dp);

	void InitLeds();
	void FlushLed();			// 刷新led状态输出

	byte LedStat(byte showmode);

	NetworkInterface* Create8266();

	void InitClient();
	void InitNet();
	void InitAlarm();
	//双联开关
	void Union(Pin pin1,Pin pin2);
	void Restore();
	void OnLongPress(InputPort* port, bool down);

	static IOK026X* Current;

private:
	void*	Data;
	int		Size;
};

#endif
