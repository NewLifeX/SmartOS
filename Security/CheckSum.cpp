#include "CheckSum.h"

byte CheckSum::Sum(const Array& data)
{
	byte sum;
	
	for (int k = 0; k < data.Length(); k++)
	{
		sum^=data[k];
	}
	
	return sum;
}
