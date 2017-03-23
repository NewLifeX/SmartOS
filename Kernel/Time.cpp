#include "TTime.h"

#include "Core\Environment.h"

#if defined(__CC_ARM)
#include <time.h>
#else
#include <ctime>
#endif

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
	Seconds = 0;
	//Ticks	= 0;
#if defined(STM32F0) || defined(GD32F150)
	Index = 13;
#else
	Div = 0;
	if (Sys.FlashSize > 0x80)
		Index = 5;
	else
		Index = 1;	// 错开开关的 TIM3 背光
#endif
	BaseSeconds = 0;

	OnInit = nullptr;
	OnLoad = nullptr;
	OnSave = nullptr;
	OnSleep = nullptr;
}

void TTime::SetTime(UInt64 sec)
{
	if (sec >= BASE_YEAR_US) sec -= BASE_YEAR_US;

	BaseSeconds = (int)(sec - Seconds);

#if DEBUG
	/*DateTime dt(sec);
	debug_printf("TTime::SetTime 设置时间 ");
	dt.Show(true);*/
#endif

	// 保存到RTC
	if (OnSave) OnSave();
}

// 关键性代码，放到开头
INROOT void TTime::Sleep(int ms, bool* running) const
{
	// 睡眠时间太短
	if (ms <= 0) return;

	// 结束时间
	UInt64 end = Current() + ms;

	// 较大的睡眠时间直接让CPU停下来
	if (OnSleep && ms >= 10)
	{
		while (ms >= 10)
		{
			OnSleep(ms);

			// 判断是否需要继续
			if (running != nullptr && !*running) break;

			// 重新计算剩下的时间
			ms = (int)(end - Current());
		}
	}
	// 睡眠时间太短
	if (!ms || (running && !*running)) return;

	// 空转
	while (true)
	{
		if (Current() >= end) break;
		if (running != nullptr && !*running) break;
	}
}

INROOT void TTime::Delay(int us) const
{
	// 睡眠时间太短
	if (us <= 0) return;

	// 当前函数耗时1~3us
	if (us > 100) us -= 1;

	UInt64 end = Current();
	if (us >= 1000)
	{
		// 拆分毫秒和微秒
		int ms = us / 1000;
		end += ms;

		us %= 1000;
	}

	// 微秒转为滴答
	uint ticks = CurrentTicks() + UsToTicks(us);
	// 结束嘀嗒数有可能超过1000
	uint max = UsToTicks(1000 - 1);
	if (ticks >= max)
	{
		end++;
		ticks -= max;
	}

	// 等待目标时间
	while (true)
	{
		// 首先比较毫秒数
		int n = (int)(Current() - end);
		if (n > 0) break;

		if (n == 0 && CurrentTicks() >= ticks) break;
	}
}

extern "C"
{
#ifndef _MSC_VER
	// 获取系统启动后经过的毫秒数
	clock_t clock(void)
	{
		return Time.Current();
	}

	// 实现C函数库的time函数
	time_t time(time_t* sec)
	{
		uint s = Time.BaseSeconds + Time.Seconds;
		if (sec) *sec = s;

		return s;
	}
#endif
}

/************************************************ TimeWheel ************************************************/

TimeWheel::TimeWheel(uint ms)
{
	Sleep = 0;
	Reset(ms);
}

void TimeWheel::Reset(uint ms)
{
	Expire = Time.Current() + ms;
}

// 是否已过期
bool TimeWheel::Expired()
{
	UInt64 now = Time.Current();
	if (now > Expire) return true;

	// 睡眠，释放CPU
	if (Sleep) Sys.Sleep(Sleep);

	return false;
}

/************************************************ TimeCost ************************************************/

TimeCost::TimeCost()
{
	Reset();
}

void TimeCost::Reset()
{
	Start = Time.Current();
	StartTicks = Time.CurrentTicks();
}

// 逝去的时间，微秒
int TimeCost::Elapsed() const
{
	int ts = (int)(Time.CurrentTicks() - StartTicks);
	int ms = (int)(Time.Current() - Start);

	// 有可能滴答部分不是完整的一圈
	if (ts > 0) return ms * 1000 + Time.TicksToUs(ts);

	// 如果毫秒部分也没有，那么可能是微小错误偏差
	if (ms <= 0) return 0;

	// 如果滴答是负数，则干脆减去
	return ms * 1000 - Time.TicksToUs(-ts);
}

void TimeCost::Show(cstring format) const
{
	if (!format) format = "执行 %dus\r\n";
	debug_printf(format, Elapsed());
}
