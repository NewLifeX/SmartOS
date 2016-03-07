#ifndef _HX711_H_
#define _HX711_H_

#include "Port.h"
#include "Power.h"

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
	bool Opened	= false;
	OutputPort * SCK	= NULL;
	IntputPort * DOUT	= NULL;
	
public:
	HX711Mode Mode = A128;

	HX711(Pin sck,Pin dout);
	
	bool Open();
	bool Close();
	
	uint Read();
};

#endif
