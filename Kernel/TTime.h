#ifndef __TIME_H__
#define __TIME_H__

#include "Sys.h"

// 时间类
// 使用双计数时钟，TIMx专用于毫秒级系统计时，SysTick用于微秒级延迟，秒级以及以上采用全局整数累加
// 这样子可以避免频繁使用微秒时带来长整型乘除法
class TTime
{
private:
    static void OnHandler(ushort num, void* param);

public:
    uint	Seconds;		// 全局秒数，系统启动后总秒数。累加
	UInt64	Milliseconds;	// 全局毫秒数，系统启动后总毫秒（1000ms整部分）。累加
    uint	BaseSeconds;	// 基准秒数。系统启动时相对于1970年的秒数，时间调节，加上Seconds得到当前时间Now()
    byte	Ticks;			// 每微秒的时钟滴答数
	byte	Index;			// 定时器
#if ! (defined(STM32F0) || defined(GD32F150))
	byte	Div;			// 分频系数。最大分频64k，无法让大于64M主频的芯片分配得到1k时钟
#endif

	Func OnInit;
	Func OnLoad;
	Func OnSave;
	typedef int (*FuncInt)(int);
	FuncInt OnSleep;

    TTime();

	void UseRTC();			// 使用RTC，必须在Init前调用
	void Init();

    uint CurrentTicks() const;	// 当前滴答时钟
	UInt64 Current() const; 		// 当前毫秒数
	void SetTime(UInt64 seconds);	// 设置时间

	void Sleep(uint ms, bool* running = nullptr) const;
	// 微秒级延迟
    void Delay(uint us) const;
};

extern const TTime Time;

// 时间轮。用于超时处理
class TimeWheel
{
public:
	uint	Expire;		// 到期时间，毫秒
	ushort	Sleep;		// 睡眠时间，默认0毫秒

	TimeWheel(uint ms);

	void Reset(uint ms);

	// 是否已过期
	bool Expired();
};

// 时间开支。借助滴答进行精确计算
class TimeCost
{
public:
	UInt64	Start;		// 开始时间，毫秒
	ushort	StartTicks;	// 开始滴答

	TimeCost();

	int Elapsed();	// 逝去的时间，微秒
	void Show(cstring format = nullptr);
};

/*
开发历史：

2015-10-05
系统最频繁使用的函数Current()存在除法性能问题，原采用滴答定时器8分频，若干个滴答才得到一个微秒，必然存在一个除法运算。
而任务调度系统等各个地方大量调用Current以获取当前时间，特别是中断函数内部直接或间接调用，造成极大的性能损耗。
根据黄总@405803243的指点，修改为双定时器架构。
系统时钟采用基本定时器，任意分频，很方便凑出每个计数表示1毫秒，彻底根除Current除法问题。
另外，滴答定时器用于微秒级高精度延迟，应用于硬件驱动开发等场合。

*/

#endif
