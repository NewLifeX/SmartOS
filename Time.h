#ifndef __TIME_H__
#define __TIME_H__

#include "Sys.h"

// 系统时钟
struct SystemTime
{
	ushort Year;
	byte Month;
	byte DayOfWeek;
	byte Day;
	byte Hour;
	byte Minute;
	byte Second;
	ushort Millisecond;
	ushort Microsecond;

	char _Str[19 + 1]; // 内部字符串缓冲区，按最长计算

	SystemTime& Parse(ulong us);
	uint TotalSeconds();
	ulong TotalMicroseconds();

	// 默认格式化时间为yyyy-MM-dd HH:mm:ss
	/*
	d短日期 M/d/yy
	D长日期 yyyy-MM-dd
	t短时间 mm:ss
	T长时间 HH:mm:ss
	f短全部 M/d/yy HH:mm
	F长全部 yyyy-MM-dd HH:mm:ss
	*/
	const char* ToString(byte kind = 'F', string str = NULL);
};

// 时间类
// 使用双计数时钟，Ticks累加滴答，Microseconds累加微秒，_usTicks作为累加微秒时的滴答余数
// 这样子可以避免频繁使用微秒时带来长整型乘除法
class TTime
{
private:
    static void OnHandler(ushort num, void* param);
	SystemTime _Now;
	uint _usTicks;	// 计算微秒时剩下的嘀嗒数

public:
    volatile ulong Ticks;  // 全局滴答中断数，0xFFFF次滴答一个中断。
	volatile ulong Microseconds;	// 全局微秒数
    //volatile ulong NextEvent;    // 下一个计划事件的嘀嗒数

    uint TicksPerSecond;        // 每秒的时钟滴答数
    ushort TicksPerMillisecond;	// 每毫秒的时钟滴答数
    byte TicksPerMicrosecond;   // 每微秒的时钟滴答数

	uint InterruptsPerSecond;	// 每秒的中断数，时间片抢占式系统调度算法基于此值调度，也即是线程时间片，默认1000

    TTime();
    virtual ~TTime();

	void Init();
    //void SetCompare(ulong compareValue);
    ulong CurrentTicks();	// 当前滴答时钟
	void SetTime(ulong us);	// 设置时间
	ulong Current(); 		// 当前微秒数
    void Sleep(uint us);

	// 当前时间。外部不要释放该指针
	SystemTime& Now();
};

extern TTime Time;

// 时间轮。用于超时处理
class TimeWheel
{
private:
	ulong _expire;

public:
	TimeWheel(uint seconds, uint ms = 0, uint us = 0)
	{
		Reset(seconds, ms, us);
	}

	void Reset(uint seconds, uint ms = 0, uint us = 0)
	{
		_expire = ((seconds * 1000) + ms) * 1000 + us;
		_expire += Time.Current();
	}

	// 是否已过期
	bool Expired()
	{
		return Time.Current() >= _expire;
	}
};

#endif
