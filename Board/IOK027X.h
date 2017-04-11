#ifndef _IOK027X_H_
#define _IOK027X_H_

#include "B8266.h"

#include "APP\Button_GrayLevel.h"

#include "Kernel\Sys.h"
#include "Net\ITransport.h"
#include "Net\Socket.h"

#include "TokenNet\TokenClient.h"
#include "App\Alarm.h"
#include "Device\RTC.h"


// WIFI触摸开关
class IOK027X : public B8266
{


public:
	List<Pin>			LedPins;
	List<OutputPort*>	Leds;

	byte LedsShow;					// LED 显示状态开关  0 刚启动时候的20秒   1 使能   2 失能

	NetworkInterface*	Host;			// 网络主机
	TokenClient*	Client;			//
	Alarm*			AlarmObj;
	uint			LedsTaskId;

	cstring			SSID;
	cstring			Pass;

	IOK027X();

	void Init(ushort code, cstring name, COM message = COM1);

	void* InitData(void* data, int size);
	void InitWiFi(cstring ssid, cstring pass);
	void Register(int index, IDataPort& dp);
	void SetRestore(Pin pin = PB4);			//设置重置引脚
	void InitLeds();
	void FlushLed();			// 刷新led状态输出

	byte LedStat(byte showmode);

	NetworkInterface* Create8266(Pin power = PB2);

	void InitClient();
	void InitNet(Pin power = PB2);
	void InitAlarm();
	//双联开关
	void Union(Pin pin1, Pin pin2);
	void Restore();
	void OnLongPress(InputPort* port, bool down);

	static IOK027X* Current;

private:
	void*	Data;
	int		Size;
	InputPort* RestPort;
	

};

#endif

