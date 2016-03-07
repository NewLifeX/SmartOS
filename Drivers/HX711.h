#ifndef _HX711_H_
#define _HX711_H_

#include "Port.h"
#include "Power.h"

// 24位 ADC
class HX711
{
private:

	
public:
	bool Opened;

	HX711();
	
	bool Open();
	bool Close();
	
	bool Read(byte *buf);

	uint Read();
};

#endif
