#include <stdlib.h>
#include <time.h>

#include "Buffer.h"

#include "Random.h"

/************************************************ Random ************************************************/
Random::Random()
{
	srand((uint)time(NULL));
}

Random::Random(int seed)
{
	srand(seed);
}

int Random::Next() const
{
	return rand();
}

int Random::Next(int max) const
{
	int value	= rand();

	if(max == 0x100) return value & 0xFF;

	return value % max;
}

void Random::Next(Buffer& bs) const
{
	for(int i=0; i<bs.Length(); i++)
		bs[i]	= rand() & 0xFF;
}
