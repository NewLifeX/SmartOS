#ifndef _Pandora_H_
#define _Pandora_H_

#include "BaseBoard.h"
#include "W5500Module.h"
#include "Esp8266Module.h"

// 潘多拉0903
class PA0903 : public BaseBoard, public W5500Module, public Esp8266Module
{
public:
	//List<OutputPort*>	Outputs;
	//List<InputPort*>	Inputs;
	
	PA0903();

	static PA0903* Current;
};

#endif
