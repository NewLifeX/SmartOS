#ifndef _W5500Module_H_
#define _W5500Module_H_

#include "Net\Socket.h"
#include "Device\Spi.h"

// W5500模块
class W5500Module
{
public:
	SpiConfig		Net;

	W5500Module();

	// 打开以太网W5500
	NetworkInterface* Create5500(OutputPort* led = nullptr);
};

#endif
