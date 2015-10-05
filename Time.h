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
    byte	Ticks;			// 每微秒的时钟滴答数
	byte	Index;			// 定时器

	Func OnInit;
	Func OnLoad;
	Func OnSave;
	typedef int (*FuncInt)(int);
	FuncInt OnSleep;

    TTime();
    //~TTime();

	void UseRTC();			// 使用RTC，必须在Init前调用
	void Init();

    uint CurrentTicks();	// 当前滴答时钟
	ulong Current(); 		// 当前毫秒数
	void SetTime(ulong ms);	// 设置时间

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
	ushort	Expire2;	// 到期时间，微秒
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
	uint	StartTicks;	// 开始滴答

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
	DateTime(ulong us);

	// 重载等号运算符
    DateTime& operator=(ulong v);

	DateTime& Parse(ulong us);
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

#endif
