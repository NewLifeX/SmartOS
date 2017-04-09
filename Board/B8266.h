#ifndef _B8266_H_
#define _B8266_H_

#include "TokenBoard.h"

#include "Net\Socket.h"

#include "Device\Spi.h"
#include "Device\SerialPort.h"

#include "App\Alarm.h"

// Esp8266核心板
class B8266 : public TokenBoard
{
public:
	Alarm*			AlarmObj;

	SerialConfig	Esp;

	cstring			SSID;
	cstring			Pass;

	B8266();

	void InitWiFi(cstring ssid, cstring pass);
	void SetRestore(Pin pin = PB4);			// 设置重置引脚

	NetworkInterface* Create8266();

	void InitNet();
	void InitAlarm();

	static B8266* Current;

private:
	InputPort* RestPort;
};

#endif
