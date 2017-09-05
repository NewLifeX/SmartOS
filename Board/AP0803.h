#ifndef _AP0803_H_
#define _AP0803_H_

#include "BaseBoard.h"
#include "GsmModule.h"

// 阿波罗0803 GPRS通信
class AP0803 : public BaseBoard, public GsmModule
{
public:
	List<OutputPort*>	Outputs;
	List<InputPort*>	Inputs;

	AP0803();

	static AP0803* Current;
};

#endif
