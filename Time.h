#ifndef __TIME_H__
#define __TIME_H__

#include "Sys.h"

// 时间类
class TTime
{
private:
    static void OnHandler(ushort num, void* param);

public:
    volatile ulong Ticks;  // 全局滴答中断数，0xFFFF次滴答一个中断。乘以0x10000，避免每次计算滴答时都需要移位
    volatile ulong NextEvent;    // 下一个计划事件的嘀嗒数

    uint TicksPerSecond;        // 每秒的时钟滴答数
    uint TicksPerMillisecond;   // 每毫秒的时钟滴答数
    uint TicksPerMicrosecond;   // 每微秒的时钟滴答数

    TTime();
    virtual ~TTime();

	void Init();
    void SetCompare(ulong compareValue);
    ulong CurrentTicks();	// 当前滴答时钟
	ulong NewTicks(uint us); // 累加指定微秒后的滴答时钟。一般用来做超时检测，直接比较滴答不需要换算更高效
	ulong CurrentMicrosecond(); // 当前微秒数
    void Sleep(uint us);
};

extern TTime Time;

#endif
