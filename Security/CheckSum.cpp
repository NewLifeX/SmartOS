#include "CheckSum.h"

byte CheckSum::Sum(const Array& data,byte check)
{
	byte s;
	
	for (int k = 0; k < data.Length(); k++)
	{
		s^=data[k];
	}
	check =s;
	return s;
}
