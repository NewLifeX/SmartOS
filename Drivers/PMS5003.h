#ifndef _PMS5003_H_
#define _PMS5003_H_

#include "Net\ITransport.h"
#include "Message\DataStore.h"

#include "BufferPort.h"

// 粉尘传感器PM1.0/PM2.5/PM10
class PMS5003 : public BufferPort
{
public:
	PMS5003();

protected:
	//virtual void OnReceive(const Array& bs, void* param);
};

#endif
