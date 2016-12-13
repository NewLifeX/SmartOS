#include "Kernel\Sys.h"
#include "Kernel\Interrupt.h"
#include "Kernel\TTime.h"
#include "Device\Timer.h"

#include "RTL871x.h"
#include "timer_api.h"

#define TIME_DEBUG 0

/************************************************ TTime ************************************************/

static gtimer_t _timer;

static void OnTimerHandler(uint param)
{
	// 重新加载，清零
	gtimer_reload(&_timer, 1000);

	auto& time	= *(TTime*)param;
	// 累加计数
	time.Seconds += 1;
	time.Milliseconds += 1000;

	// 定期保存Ticks到后备RTC寄存器
	if(time.OnSave) time.OnSave();
}

void TTime::Init()
{
	auto ti	= &_timer;
	gtimer_init(ti, TIMER0);
	gtimer_start_periodical(ti, 1000, (void*)OnTimerHandler, (uint)this);
}

void TTime::OnHandler(ushort num, void* param)
{
}

// 当前滴答时钟
uint TTime::CurrentTicks() const
{
	return gtimer_read_tick(&_timer);
}

// 当前毫秒数
UInt64 TTime::Current() const
{
	UInt64 ms	= gtimer_read_tick(&_timer) * 1000;
	ms >>= 15;

	return Milliseconds + ms;
}
