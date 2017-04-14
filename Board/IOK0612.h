#ifndef _IOK0612_H_
#define _IOK0612_H_

#include "B8266.h"

// WIFI WRGB调色
class IOK0612 : public B8266
{
public:
	IOK0612();

	static IOK0612* Current;
};

#endif
