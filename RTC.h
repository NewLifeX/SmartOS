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
	int _TickTime;  // 几秒中断一次
	
public:
	void Register(RTCHandler handler, void* param = NULL);
	int GetTime();
	void SerTime(int );

	RTCHandler  _Handler;
	void * _param;
private:
	
};


#endif
