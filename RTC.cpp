#include "RTC.h"



//	InterruptCallback RCC_Handle;
//  typedef void (*InterruptCallback)(ushort num, void* param);
//  我去 还得用全局函数
void RCC_Handler(ushort num, void* param);



RTClock::RTClock()
{
	_Handler =NULL;
	_param=NULL;
}

RTClock::RTClock(int s)
{
	
}

void RTClock::Register(RTCHandler handler, void* param)
{
	Interrupt.SetPriority(RCC_IRQn, 1);
	if(param != NULL)
	{
		_Handler = handler;
		_param = param;
		Interrupt.Activate(RCC_IRQn,RCC_Handler,this);
	}
}

void RCC_Handler(ushort num, void* param)
{
	RTClock * rtc = (RTClock *)param;
	if(param != NULL)
		rtc->_Handler(rtc->_param);
}



