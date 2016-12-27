#ifndef _IOK027X_H_
#define _IOK027X_H_

#include "Sys.h"
#include "Net\ITransport.h"

#include "TokenNet\TokenClient.h"
#include "App\Alarm.h"
#include "APP\Button_GrayLevel.h"
#include "Device\RTC.h"

// WIFI触摸开关 123位
class IOK027X
{
public:
	List<Pin>	LedPins;
	List<OutputPort*>	Leds;

	byte LedsShow;					// LED 显示状态开关  0 刚启动时候的20秒   1 使能   2 失能

	ISocketHost*	Host;			// 网络主机
	TokenClient*	Client;			//
	Alarm*			AlarmObj;
	uint			LedsTaskId;

	IOK027X();

	void Init(ushort code, cstring name, COM message = COM1);

	void* InitData(void* data, int size);
	void Register(int index, IDataPort& dp);

	void InitLeds();
	void FlushLed();			// 刷新led状态输出

	byte LedStat(byte showmode);

	ISocketHost* Create8266(Pin power=PB2);

	void InitClient();
	void InitNet(Pin power=PB2);
	void InitAlarm();
	//双联开关
	void Union(Pin pin1, Pin pin2);
	void Restore();
	void OnLongPress(InputPort* port, bool down);

	static IOK027X* Current;

private:
	void*	Data;
	int		Size;

	void OpenClient(ISocketHost& host);
	TokenController* AddControl(ISocketHost& host, const NetUri& uri, ushort localPort);
};

#endif
