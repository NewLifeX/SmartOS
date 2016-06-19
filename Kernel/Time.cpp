#include <time.h>

#include "Time.h"

#include "Environment.h"

#define TIME_DEBUG 0

#define UTC		true								// utc 从1970/1/1 开始计时
#define UTC_CALIBRATE				946684800UL		// 2000/1/1 - 1970/1/1 秒值
#ifdef UTC
#define BASE_YEAR_US				62135596800UL		// (63082281600UL-UTC_CALIBRATE)	// 从0 到 2000-01-01的所有秒数
#else
#define BASE_YEAR_US				63082281600UL	// 从0 到 2000-01-01的所有秒数
#endif
/************************************************ TTime ************************************************/

TTime::TTime()
{
	Seconds	= 0;
	Ticks	= 0;
#if defined(STM32F0) || defined(GD32F150)
	Index	= 13;
#else
	Div		= 0;
	if(Sys.FlashSize > 0x80)
		Index	= 5;
	else
		Index	= 2;
#endif
	BaseSeconds = 0;

	OnInit	= nullptr;
	OnLoad	= nullptr;
	OnSave	= nullptr;
	OnSleep	= nullptr;
}

void TTime::SetTime(UInt64 seconds)
{
	if(seconds >= BASE_YEAR_US) seconds -= BASE_YEAR_US;

	BaseSeconds = seconds - Seconds;

#if DEBUG
	/*DateTime dt(seconds);
	debug_printf("TTime::SetTime 设置时间 ");
	dt.Show(true);*/
#endif

	// 保存到RTC
	if(OnSave) OnSave();
}

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code = "SectionForSys"
	#elif defined(__GNUC__)
		__attribute__((section("SectionForSys")))
	#endif
#endif

void TTime::Sleep(uint ms, bool* running) const
{
    // 睡眠时间太短
    if(!ms) return;

	// 较大的睡眠时间直接让CPU停下来
	if(OnSleep && ms >= 10)
	{
		uint ms2 = ms;
		if(OnSleep(ms2) == 0)
		{
			// CPU睡眠是秒级，还有剩余量
			if(ms >= ms2)
				ms -= ms2;
			else
				ms = 0;
		}
	}
    // 睡眠时间太短
    if(!ms || (running && !*running)) return;

	uint me	= Current() + ms;

    while(true)
	{
		if(Current() >= me) break;

		if(running != nullptr && !*running) break;
	}
}

void TTime::Delay(uint us) const
{
    // 睡眠时间太短
    if(!us) return;

	// 较大的时间，按毫秒休眠
	if(us >= 1000)
	{
		Sleep(us / 1000);
		us %= 1000;
	}
    // 睡眠时间太短
    if(!us) return;

	// 无需关闭中断，也能实现延迟
	UInt64 ms	= Current();
	uint ticks	= CurrentTicks() + us * Ticks;
	if(ticks >= (1000 - 1) * Ticks)
	{
		ms++;
		ticks -= (1000 - 1) * Ticks;
	}

    while(true)
	{
		int n = Current() - ms;
		if(n > 0) break;
		if(n == 0 && CurrentTicks() >= ticks) break;
	}
}

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code
	#elif defined(__GNUC__)
		__attribute__((section("")))
	#endif
#endif

extern "C"
{
	// 获取系统启动后经过的毫秒数
	clock_t clock(void)
	{
		return Time.Current();
	}

	// 实现C函数库的time函数
	time_t time(time_t* seconds)
	{
		uint s	= Time.BaseSeconds + Time.Seconds;
		if(seconds) *seconds	= s;

		return s;
	}
}

/************************************************ TimeWheel ************************************************/

TimeWheel::TimeWheel(uint seconds, uint ms, uint us)
{
	Sleep = 0;
	Reset(seconds, ms, us);
}

void TimeWheel::Reset(uint seconds, uint ms, uint us)
{
	Expire	= Time.Current() + seconds * 1000 + ms;
	Expire2	= Time.CurrentTicks() + us * Time.Ticks;
}

// 是否已过期
bool TimeWheel::Expired()
{
	UInt64 now = Time.Current();
	if(now > Expire) return true;
	if(now == Expire && Time.CurrentTicks() >= Expire2) return true;

	// 睡眠，释放CPU
	if(Sleep) Sys.Sleep(Sleep);

	return false;
}

/************************************************ TimeCost ************************************************/

TimeCost::TimeCost()
{
	Start		= Time.Current();
	StartTicks	= Time.CurrentTicks();
}

// 逝去的时间，微秒
int TimeCost::Elapsed()
{
	short ts	= Time.CurrentTicks() - StartTicks;
	int ms		= Time.Current() - Start;

	ts /= Time.Ticks;
	if(ts <= 0) ts += 1000;

	return ms * 1000 + ts;
}

void TimeCost::Show(cstring format)
{
	if(!format) format = "执行 %dus\r\n";
	debug_printf(format, Elapsed());
}
