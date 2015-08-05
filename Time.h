#ifndef __TIME_H__
#define __TIME_H__

#include "Sys.h"

// 系统时钟
struct DateTime
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

	DateTime();

	// 重载等号运算符
    DateTime& operator=(ulong v);

	DateTime& Parse(ulong us);
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
	DateTime _Now;
	volatile uint _usTicks;			// 计算微秒时剩下的滴答数
	volatile uint _msUs;			// 计算毫秒时剩下的微秒数
	Func OnInit;
	Func OnLoad;
	Func OnSave;
	Action OnSleep;

public:
    volatile ulong Ticks;			// 全局滴答中断数，0xFFFF次滴答一个中断。
	volatile ulong Microseconds;	// 全局微秒数
	volatile ulong Milliseconds;	// 全局毫秒数
    //volatile ulong NextEvent;		// 下一个计划事件的滴答数

    byte	TicksPerMicrosecond;	// 每微秒的时钟滴答数

    TTime();
    ~TTime();

	void UseRTC();					// 使用RTC，必须在Init前调用
	void Init();
	void SetMax(uint usMax);		// 设置多少个微秒发生一次中断。基于时间片抢占式系统基于此调度
    //void SetCompare(ulong compareValue);

    ulong CurrentTicks();	// 当前滴答时钟
	ulong Current(); 		// 当前微秒数
	void SetTime(ulong us);	// 设置时间
    void Sleep(uint us, bool* running = NULL);
	void LowPower();		// 启用低功耗模式，Sleep时进入睡眠

	// 当前时间。外部不要释放该指针
	DateTime& Now();
};

extern TTime Time;

// 时间轮。用于超时处理
class TimeWheel
{
public:
	ulong	Expire;		// 到期时间，滴答
	uint	Sleep;		// 睡眠时间，默认0毫秒

	TimeWheel(uint seconds, uint ms = 0, uint us = 0);

	void Reset(uint seconds, uint ms = 0, uint us = 0);

	// 是否已过期
	bool Expired();
};

// 时间开支。借助滴答进行精确计算
class TimeCost
{
public:
	ulong Start;	// 开始时间，滴答

	TimeCost();

	int Elapsed();	// 逝去的时间，微秒
	void Show(const char* format = NULL);
};

#endif
