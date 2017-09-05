#ifndef _AP0801_H_
#define _AP0801_H_

#include "BaseBoard.h"
#include "W5500Module.h"
#include "Esp8266Module.h"

// 阿波罗0801
class AP0801 : public BaseBoard, public W5500Module, public Esp8266Module
{
public:
	List<OutputPort*>	Outputs;
	List<InputPort*>	Inputs;

	AP0801();

	static AP0801* Current;
};

#endif
