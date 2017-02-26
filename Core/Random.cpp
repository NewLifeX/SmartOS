#include <stdlib.h>
#include <time.h>

#include "Buffer.h"

#include "Random.h"

/************************************************ Random ************************************************/
Random::Random()
{
	srand((uint)time(NULL));
}

Random::Random(uint seed)
{
	srand(seed);
}

uint Random::Next() const
{
	return rand();
}

uint Random::Next(uint max) const
{
	uint value	= rand();

	if(max == 0x100) return value & 0xFF;

	return value % max;
}

void Random::Next(Buffer& bs) const
{
	for(int i=0; i<bs.Length(); i++)
		bs[i]	= rand() & 0xFF;
}
