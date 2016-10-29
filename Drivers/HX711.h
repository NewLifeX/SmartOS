#ifndef _HX711_H_
#define _HX711_H_

#include "Device\Port.h"
#include "Device\Power.h"

enum HX711Mode
{
	A128	= 1,	// A通道128倍
	A64		= 2,	// B通道32 倍
	B32		= 3,	// A通道64 倍
};

// 24位 ADC
class HX711
{
private:
	OutputPort	SCK;
	InputPort	DOUT;
	bool Opened;

public:
	HX711Mode Mode = A128;

	HX711(Pin sck, Pin dout);
	//~HX711();

	bool Open();
	bool Close();

	uint Read();
};

#endif
