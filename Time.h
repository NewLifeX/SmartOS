#ifndef __TIME_H__
#define __TIME_H__

#include "Sys.h"

// 时间类
class Time
{
private:
    static void OnHandler();

public:
    ulong Ticks;  // 全局滴答中断数，0xFFFF次滴答一个中断。乘以0x10000，避免每次计算滴答时都需要移位
    ulong NextEvent;    // 下一个计划事件的嘀嗒数

    uint TicksPerSecond;        // 每秒的时钟滴答数
    uint TicksPerMillisecond;   // 每毫秒的时钟滴答数
    uint TicksPerMicrosecond;   // 每微秒的时钟滴答数

    Time();
    virtual ~Time();

    void SetCompare(ulong compareValue);
    ulong CurrentTicks();
    void Sleep(uint us);
};

extern Time* g_Time;

#endif
