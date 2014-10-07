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
	Timer(TIM_TypeDef* timer);
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

public:
	static Timer**		Timers;		// 已经实例化的定时器对象
	static const byte	TimerCount;	// 定时器个数

	static Timer* Create(byte index = 0xFF);	// 创建指定索引的定时器，如果已有则直接返回，默认0xFF表示随机分配
};

#endif
