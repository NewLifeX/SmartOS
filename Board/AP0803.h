#ifndef _AP0803_H_
#define _AP0803_H_

#include "BaseBoard.h"
#include "GsmModule.h"

//#include "App\Alarm.h"

// 阿波罗0803 GPRS通信
class AP0803 : public BaseBoard, public GsmModule
{
public:
	List<OutputPort*>	Outputs;
	List<InputPort*>	Inputs;

	//Alarm*			AlarmObj;

	AP0803();

	//void InitProxy();
	//void InitAlarm();

	static AP0803* Current;
};

#endif
