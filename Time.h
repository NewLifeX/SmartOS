#ifndef __TIME_H__
#define __TIME_H__

#include "Sys.h"

class DateTime;

// 时间类
// 使用双计数时钟，TIMx专用于毫秒级系统计时，SysTick用于微秒级延迟，秒级以及以上采用全局整数累加
// 这样子可以避免频繁使用微秒时带来长整型乘除法
class TTime
{
private:
    static void OnHandler(ushort num, void* param);

public:
    uint	Seconds;		// 全局秒数。累加
	ulong	Milliseconds;	// 全局毫秒数。累加
    uint	BaseSeconds;	// 基准秒数。时间调节，影响Now()
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

    uint CurrentTicks();	// 当前滴答时钟
	ulong Current(); 		// 当前毫秒数
	void SetTime(ulong seconds);	// 设置时间

	void Sleep(uint ms, bool* running = NULL);
	// 微秒级延迟
    void Delay(uint us);

	// 当前时间。
	DateTime Now();
};

extern TTime Time;

// 时间轮。用于超时处理
class TimeWheel
{
public:
	uint	Expire;		// 到期时间，毫秒
	ushort	Expire2;	// 到期时间，滴答
	ushort	Sleep;		// 睡眠时间，默认0毫秒

	TimeWheel(uint seconds, uint ms = 0, uint us = 0);

	void Reset(uint seconds, uint ms = 0, uint us = 0);

	// 是否已过期
	bool Expired();
};

// 时间开支。借助滴答进行精确计算
class TimeCost
{
public:
	ulong	Start;		// 开始时间，毫秒
	ushort	StartTicks;	// 开始滴答

	TimeCost();

	int Elapsed();	// 逝去的时间，微秒
	void Show(const char* format = NULL);
};

// 系统时钟
class DateTime : public Object
{
public:
	ushort Year;
	byte Month;
	byte DayOfWeek;
	byte Day;
	byte Hour;
	byte Minute;
	byte Second;
	ushort Millisecond;
	ushort Microsecond;

	DateTime();
	DateTime(ulong seconds);

	// 重载等号运算符
    DateTime& operator=(ulong seconds);

	DateTime& Parse(ulong seconds);
	DateTime& ParseUs(ulong us);
	uint TotalSeconds();
	ulong TotalMicroseconds();

	virtual String& ToStr(String& str) const;

	// 默认格式化时间为yyyy-MM-dd HH:mm:ss
	/*
	d短日期 M/d/yy
	D长日期 yyyy-MM-dd
	t短时间 mm:ss
	T长时间 HH:mm:ss
	f短全部 M/d/yy HH:mm
	F长全部 yyyy-MM-dd HH:mm:ss
	*/
	const char* GetString(byte kind = 'F', string str = NULL);
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
