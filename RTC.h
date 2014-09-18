#ifndef __RTC_H__
#define __RTC_H__

#include "Sys.h"
#include "Interrupt.h"


// RTC
class RTClock
{
    typedef void (*RTCHandler)(void* param);
	
	RTClock();
	RTClock(int s);

public:
	void Register(RTCHandler handler, void* param = NULL);


	RTCHandler  _Handler;
	void * _param;
private:
	
// ÖÐ¶ÏºÅ  ²ÎÊý
};


#endif
