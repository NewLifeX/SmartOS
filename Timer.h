#ifndef __Timer_H__
#define __Timer_H__

#include "Sys.h"

// 定时器
class Timer
{
private:
	TIM_TypeDef* _port;
	byte _index;	// 第几个定时器，从0开始
	bool _started;

	void ClockCmd(bool state);
public:
	Timer(byte index);	// 第几个定时器，从1开始
	~Timer();

	ushort Prescaler;	// 预分频。实际值，此时无需减一。默认预分配到1MHz
	uint Period;	// 周期。实际值，此时无需减一。默认1000个周期

	void Start();	// 开始定时器
	void Stop();	// 停止定时器
	//void SetScaler(uint scaler);	// 设置预分频目标，比如1MHz
	void SetFrequency(uint frequency);	// 设置频率，自动计算预分频

	void Register(EventHandler handler, void* param = NULL);

private:
	void OnInterrupt();
	static void OnHandler(ushort num, void* param);
	EventHandler _Handler;
	void* _Param;
};

#endif
