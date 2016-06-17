#ifndef __Timer_H__
#define __Timer_H__

#include "Type.h"
#include "Delegate.h"
#include "Port.h"

// 定时器
class Timer
{
protected:
	byte	_index;		// 第几个定时器，从0开始
	Delegate	OnTick;	// 带this参数

	void SetHandler(bool set);
public:
	void*	_Timer;
	bool	Opened;	// 可能在中断里关闭自己

	ushort	Prescaler;	// 预分频。实际值，此时无需减一。
	uint	Period;		// 周期。实际值，此时无需减一。

	Timer(TIMER index);
	virtual ~Timer();

	virtual void Open();	// 开始定时器
	virtual void Close();	// 停止定时器
	virtual void Config();
	//void SetScaler(uint scaler);	// 设置预分频目标，比如1MHz
	void SetFrequency(uint frequency);	// 设置频率，自动计算预分频

	uint GetCounter();
	void SetCounter(uint cnt);		// 设置计数器值

	//void Register(EventHandler handler, void* param = nullptr);
	void Register(const Delegate& dlg);

	static void ClockCmd(int idx, bool state);

private:
	static void OnHandler(ushort num, void* param);
	//EventHandler _Handler;
	//void* _Param;

protected:
	virtual void OnInterrupt();

public:
	static const byte	TimerCount;	// 定时器个数

	static Timer* Create(byte index = 0xFF);	// 创建指定索引的定时器，如果已有则直接返回，默认0xFF表示随机分配

private:
	void OnInit();
	void OnOpen();
	void OnClose();

	static const void* GetTimer(byte idx);
};

// 脉冲宽度调制
class PWM : public Timer
{
protected:

private:
	// 是否已配置 从低到高 4位 分别对应4个通道
	byte Configed;

public:
	ushort	Pulse[4];	// 每个通道的占空比，默认0xFFFF表示不使用该通道
	bool	Polarity	= true;	// 极性。默认true高电平
	bool	IdleState	= true;	// 空闲状态。

	PWM(TIMER index);		// index 定时器编号

	virtual void Open();
	virtual void Close();
	virtual void Config();
	// 刷新输出
	void FlushOut();

	void SetPulse(int idx, ushort pulse);

// 连续调整占空比
public:
	ushort* Pulses;		// 宽度数组
	byte	PulseCount;	// 宽度个数
	byte	Channel;	// 需要连续调整的通道。仅支持连续调整1个通道。默认0表示第一个通道
	byte	PulseIndex;	// 索引。使用数组中哪一个位置的数据
	bool	Repeated;	// 是否重复

protected:
	virtual void OnInterrupt();
};
/*
class Capture
{
private:
	Timer * _timer;
public:
//	volatile byte HaveCap;		// 用位域可能比较好   低四位分别代表一路
//	直接使用 stm32 的事件标志
//	FlagStatus TIM_GetFlagStatus(TIM_TypeDef* TIMx, uint16_t TIM_FLAG);
//	volatile int CapValue[4];	// 一个定时器又是四路

	Capture(Timer * timer = nullptr);
	~Capture();
	uint GetCapture(int channel);

	void Start(int channel);
	void Stop(int channel);
private :
	static void OnHandler(ushort num, void* param);
	void OnInterrupt();
	EventHandler _Handler[4];
	void* _Param[4];

public :
	void Register(int Index,EventHandler handler, void* param = nullptr);
};
//void (*EventHandler)(void* sender, void* param);
*/
#endif
