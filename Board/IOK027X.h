#ifndef _IOK027X_H_
#define _IOK027X_H_

#include "B8266.h"

#include "APP\Button_GrayLevel.h"

// WIFI触摸开关
class IOK027X : public B8266
{
public:
	IOK027X();

	// 联动开关
	void Union(Pin pin1, Pin pin2, bool invert);

	static IOK027X* Current;
};

#endif
