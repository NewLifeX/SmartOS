#ifndef _Esp8266Module_H_
#define _Esp8266Module_H_

#include "Net\Socket.h"
#include "Device\SerialPort.h"

// Esp8266模块
class Esp8266Module
{
public:
	SerialConfig	Esp;

	cstring			SSID;
	cstring			Pass;

	Esp8266Module();

	void InitWiFi(cstring ssid, cstring pass);

	// 打开Esp8266，作为主控或者纯AP
	NetworkInterface* Create8266(OutputPort* led = nullptr);
};

#endif
