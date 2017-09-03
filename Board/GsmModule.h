#ifndef _GsmModule_H_
#define _GsmModule_H_

#include "Net\Socket.h"
#include "Device\SerialPort.h"

// GSM模块
class GsmModule
{
public:
	SerialConfig	Gsm;

	GsmModule();

	// 打开GPRS
	NetworkInterface* CreateA67(OutputPort* led = nullptr);
	NetworkInterface* CreateSIM900A(OutputPort* led = nullptr);
};

#endif
