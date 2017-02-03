#ifndef _NH3_0317_H_
#define _NH3_0317_H_

#include "Sys.h"
#include "Net\ITransport.h"
#include "Net\Socket.h"

#include "TokenNet\TokenClient.h"
#include "App\Alarm.h"
#include "Device\RTC.h"

// 氨气
class NH3_0317
{
public:
	List<Pin>	LedPins;
	List<OutputPort*>	Leds;
	List<Pin>	ButtonPins;
	List<InputPort*>	Buttons;

	bool LedsShow;					// LED 显示状态开关

	NetworkInterface*	Host;			// 网络主机
	TokenClient*	Client;			//
	Alarm*			AlarmObj;
	uint			LedsTaskId;

	NH3_0317();

	void Init(ushort code, cstring name, COM message = COM1);

	void* InitData(void* data, int size);
	void Register(int index, IDataPort& dp);

	void InitLeds();
	void FlushLed();			// 刷新led状态输出
	void InitButtons(const Delegate2<InputPort&, bool>& press);

	bool LedStat(bool enable);

	NetworkInterface* Create8266();

	void SetStore(void* data, int len);

	void InitClient();
	void InitNet();
	void InitAlarm();
	void Invoke(const String& ation, const Buffer& bs);
    void Restore();
	static void OnLongPress(InputPort* port, bool down);

	static NH3_0317* Current;

private:
	void*	Data;
	int		Size;

	void OpenClient(NetworkInterface& host);
	TokenController* AddControl(NetworkInterface& host, const NetUri& uri, ushort localPort);
};

#endif
