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
class TTime
{
private:
    static void OnHandler(ushort num, void* param);
	SystemTime _Now;

public:
    volatile ulong Ticks;  // 全局滴答中断数，0xFFFF次滴答一个中断。乘以0x10000，避免每次计算滴答时都需要移位
    volatile ulong NextEvent;    // 下一个计划事件的嘀嗒数

    uint TicksPerSecond;        // 每秒的时钟滴答数
    ushort TicksPerMillisecond;	// 每毫秒的时钟滴答数
    byte TicksPerMicrosecond;   // 每微秒的时钟滴答数

    TTime();
    virtual ~TTime();

	void Init();
    void SetCompare(ulong compareValue);
    ulong CurrentTicks();	// 当前滴答时钟
	void SetTime(ulong us);	// 设置时间
	ulong NewTicks(uint us); // 累加指定微秒后的滴答时钟。一般用来做超时检测，直接比较滴答不需要换算更高效
	ulong Current(); // 当前微秒数
    void Sleep(uint us);

	// 微秒转为系统时钟结构体
	//SystemTime& To(ulong us, SystemTime& st);
	// 系统时钟结构体转为微秒
	//ulong From(const SystemTime& st);
	// 当前时间。外部不要释放该指针
	SystemTime& Now();
};

extern TTime Time;

#endif
