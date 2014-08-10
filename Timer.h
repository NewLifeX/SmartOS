#ifndef __Timer_H__
#define __Timer_H__

#include "Sys.h"

// 定时器
class Timer
{
private:
	TIM_TypeDef* _port;
	TIM_TimeBaseInitTypeDef _timer;
	byte _index;	// 第几个定时器，从0开始
	bool _started;

	
public:
	Timer(byte index);
	~Timer();
	
	uint Prescaler;	// 预分频。实际值，此时无需减一。默认预分配到1MHz
	uint Period;	// 周期。实际值，此时无需减一。默认1000个周期
	
	void Start();	// 开始定时器
	void Stop();	// 停止定时器
	
	typedef void (*TimerHandler)(Timer* tim, void* param);
	void Register(TimerHandler handler, void* param = NULL);

private:
	static void OnHandler(ushort num, void* param);
	TimerHandler _Handler;
	void* _Param;
};

#endif
