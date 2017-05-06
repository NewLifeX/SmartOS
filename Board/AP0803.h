#ifndef _AP0803_H_
#define _AP0803_H_

#include "TokenBoard.h"

#include "Net\Socket.h"

#include "Device\SerialPort.h"
#include "App\Alarm.h"

// 阿波罗0803 GPRS通信
class AP0803 : public TokenBoard
{
public:
	List<OutputPort*>	Outputs;
	List<InputPort*>	Inputs;

	Alarm*			AlarmObj;

	SerialConfig	Gsm;

	AP0803();

	// 打开GPRS
	NetworkInterface* CreateGPRS();

	void InitNet();
	void InitProxy();
	void InitAlarm();
};

#endif
