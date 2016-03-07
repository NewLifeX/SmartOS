#ifndef _XH711_H_
#define _XH711_H_

#include "Port.h"
#include "Power.h"

// 24位 ADC
class XH711
{
private:

	
public:
	bool Opened;

	XH711();
	
	bool Open();
	bool Close();
	
	bool Read(byte *buf);

	uint Read();
};

#endif
