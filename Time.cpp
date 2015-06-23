#include "Time.h"

#define TIME_Completion_IdleValue 0xFFFFFFFFFFFFFFFFull

#define SYSTICK_MAXCOUNT       SysTick_LOAD_RELOAD_Msk	//((1<<24) - 1)	/* SysTick MaxCount */
#define SYSTICK_ENABLE         SysTick_CTRL_ENABLE_Msk	//     0		/* Config-Bit to start or stop the SysTick Timer */

TTime::TTime()
{
	Ticks = 0;
	//NextEvent = TIME_Completion_IdleValue;

	Microseconds = 0;
	_usTicks = 0;
	Milliseconds = 0;
	_msUs = 0;

	InterruptsPerSecond = 100;
}

TTime::~TTime()
{
    Interrupt.Deactivate(SysTick_IRQn);
    // 关闭定时器
	SysTick->CTRL &= ~SYSTICK_ENABLE;
}

void TTime::Init()
{
	//RCC_ClocksTypeDef  clock;
	//RCC_GetClocksFreq(&clock);	// 获得系统时钟频率。

	// 准备使用外部时钟，Systick时钟=HCLK/8
	uint clk = Sys.Clock / 8;
	// 48M时，每秒48M/8=6M个滴答，1us=6滴答
	// 72M时，每秒72M/8=9M个滴答，1us=9滴答
	// 96M时，每秒96M/8=12M个滴答，1us=12滴答
	// 120M时，每秒120M/8=15M个滴答，1us=15滴答
	// 168M时，每秒168M/8=21M个滴答，1us=21滴答
	//TicksPerSecond = Sys.Clock / 8;		// 每秒的嘀嗒数
	//TicksPerMillisecond = TicksPerSecond / 1000;	// 每毫秒的嘀嗒数
	TicksPerMicrosecond = clk / 1000000;	// 每微秒的时钟滴答数

	/*SysTick->CTRL &= ~SysTick_CTRL_CLKSOURCE_Msk;	// 选择外部时钟，每秒有个HCLK/8滴答
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;		// 开启定时器减到0后的中断请求

	// 加载嘀嗒数，72M时~=0x00FFFFFF/9M=1864135us，96M时~=0x00FFFFFF/12M=1398101us
	SysTick->LOAD = SYSTICK_MAXCOUNT - 1;
	SysTick->VAL = 0;
	SysTick->CTRL |= SYSTICK_ENABLE;	// SysTick使能*/

	// InterruptsPerSecond单位：中断每秒，clk单位：滴答每秒，ticks单位：滴答每中断
	// 默认100，也即是每秒100次中断，10ms一次
	uint ticks = clk / InterruptsPerSecond;
	// ticks为每次中断的嘀嗒数，也就是重载值
	assert_param(ticks > 0 && ticks < SYSTICK_MAXCOUNT);
	SysTick_Config(ticks);

	// 必须放在SysTick_Config后面，因为它设为不除以8
	//SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
	// 本身主频已经非常快，除以8分频吧
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);

	// 需要最高优先级
    Interrupt.SetPriority(SysTick_IRQn, 0);
	Interrupt.Activate(SysTick_IRQn, OnHandler, this);
}

#if defined(STM32F0) || defined(STM32F4)
    #define SysTick_CTRL_COUNTFLAG SysTick_CTRL_COUNTFLAG_Msk
#endif
void TTime::OnHandler(ushort num, void* param)
{
	SmartIRQ irq;

	//uint value = (SysTick->LOAD - SysTick->VAL);
	//SysTick->VAL = 0;
	// 此时若修改寄存器VAL，将会影响SysTick_CTRL_COUNTFLAG标识位

	// 累加计数
	if(SysTick->CTRL & SysTick_CTRL_COUNTFLAG)
	{
		uint value = SysTick->LOAD + 1;

		Time.Ticks += value;

		Time._usTicks += value;
		uint us = Time._usTicks / Time.TicksPerMicrosecond;
		Time.Microseconds += us;
		Time._usTicks %= Time.TicksPerMicrosecond;

		Time._msUs += us;
		Time.Milliseconds += Time._msUs / 1000;
		Time._msUs %= 1000;

		if(Sys.OnTick) Sys.OnTick();
	}
}

/*void TTime::SetCompare(ulong compareValue)
{
    SmartIRQ irq;

    NextEvent = compareValue;

	ulong curTicks = CurrentTicks();

	uint diff;

	// 如果已经超过计划比较值，那么安排最小滴答，准备马上中断
	if(curTicks >= NextEvent)
		diff = 1;
	// 计算下一次中断的间隔，最大为SYSTICK_MAXCOUNT
	else if((compareValue - curTicks) > SYSTICK_MAXCOUNT)
		diff = SYSTICK_MAXCOUNT;
	else
		diff = (uint)(compareValue - curTicks);

	// 把时钟里面的剩余量累加到g_Ticks
	Ticks = CurrentTicks();

	// 重新设定重载值，下一次将在该值处中断
	SysTick->LOAD = diff;
	SysTick->VAL = 0x00;
}*/

ulong TTime::CurrentTicks()
{
    //SmartIRQ irq;

	uint value = (SysTick->LOAD - SysTick->VAL);

	return Ticks + value;
}

// 当前微秒数
ulong TTime::Current()
{
	uint value = (SysTick->LOAD - SysTick->VAL);

	return Microseconds + (_usTicks + value) / TicksPerMicrosecond;
}

void TTime::SetTime(ulong us)
{
    SmartIRQ irq;

	SysTick->VAL = 0;
	SysTick->CTRL &= ~SysTick_CTRL_COUNTFLAG;
	Ticks = us * TicksPerMicrosecond;

	// 修改系统启动时间
	Sys.StartTime += us - Microseconds;

	Microseconds = us;
	_usTicks = 0;

	Milliseconds = us / 1000;
	_msUs = us % 1000;
}

#define STM32_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS 3

void TTime::Sleep(uint us)
{
    // 睡眠时间太短
    if(us <= STM32_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS) return ;

	// 自己关闭中断，简直实在找死！
	// Sleep的时候，尽量保持中断打开，否则g_Ticks无法累加，从而造成死循环
	// 记录现在的中断状态
    SmartIRQ irq(true);

	// 时钟滴答需要采用UINT64
    ulong maxDiff = (ulong)us * TicksPerMicrosecond;
    ulong current = CurrentTicks();
    //ulong maxDiff = (ulong)us;
    //ulong current = Current();

	// 减去误差指令周期，在获取当前时间以后多了几个指令
    if(maxDiff <= STM32_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS)
		maxDiff  = 0;
    else
		maxDiff -= STM32_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS;

	maxDiff += current;

    while(CurrentTicks() <= maxDiff);
    //while(Current() <= maxDiff);
}

/// 我们的时间起点是 1/1/2000 00:00:00.000.000 在公历里面1/1/2000是星期六
#define BASE_YEAR                   2000
#define BASE_YEAR_LEAPYEAR_ADJUST   484
#define BASE_YEAR_DAYOFWEEK_SHIFT   6		// 星期偏移

const int CummulativeDaysForMonth[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

#define IS_LEAP_YEAR(y)             (((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0))
#define NUMBER_OF_LEAP_YEARS(y)     ((((y - 1) / 4) - ((y - 1) / 100) + ((y - 1) / 400)) - BASE_YEAR_LEAPYEAR_ADJUST) // 基于基本年1601的闰年数，不包含当年
#define NUMBER_OF_YEARS(y)          (y - BASE_YEAR)

#define YEARS_TO_DAYS(y)            ((NUMBER_OF_YEARS(y) * 365) + NUMBER_OF_LEAP_YEARS(y))
#define MONTH_TO_DAYS(y, m)         (CummulativeDaysForMonth[m - 1] + ((IS_LEAP_YEAR(y) && (m > 2)) ? 1 : 0))

DateTime& DateTime::Parse(ulong us)
{
	DateTime& st = *this;

	// 分别计算毫秒、秒、分、时，剩下天数
	uint time = us % 60000000; // 此时会削去高位，ulong=>uint
    st.Microsecond = time % 1000;
    time /= 1000;
    st.Millisecond = time % 1000;
    time /= 1000;
    st.Second = time % 60;
    //time /= 60;
	time = us / 60000000;	// 用一次大整数除法，重新计算高位
    st.Minute = time % 60;
    time /= 60;
    st.Hour = time % 24;
    time /= 24;

	// 基本年的一天不一定是星期天，需要偏移BASE_YEAR_DAYOFWEEK_SHIFT
    st.DayOfWeek = (time + BASE_YEAR_DAYOFWEEK_SHIFT) % 7;
    st.Year = (ushort)(time / 365 + BASE_YEAR);

	// 按最小每年365天估算，如果不满当天总天数，年份减一
    int ytd = YEARS_TO_DAYS(st.Year);
    if (ytd > time)
    {
        st.Year--;
        ytd = YEARS_TO_DAYS(st.Year);
    }

	// 减去年份的天数
    time -= ytd;

	// 按最大每月31天估算，如果超过当月总天数，月份加一
    st.Month = (ushort)(time / 31 + 1);
    int mtd = MONTH_TO_DAYS(st.Year, st.Month + 1);
    if (time >= mtd) st.Month++;

	// 计算月份表示的天数
    mtd = MONTH_TO_DAYS(st.Year, st.Month);

	// 今年总天数减去月份天数，得到该月第几天
    st.Day = (ushort)(time - mtd + 1);

	return st;
}

uint DateTime::TotalSeconds()
{
	uint s = 0;
	s += YEARS_TO_DAYS(Year) + MONTH_TO_DAYS(Year, Month) + Day - 1;
	s = s * 24 + Hour;
	s = s * 60 + Minute;
	s = s * 60 + Second;

	return s;
}

ulong DateTime::TotalMicroseconds()
{
	ulong us = (ulong)TotalSeconds();
	us = us * 1000 + Millisecond;
	us = us * 1000 + Microsecond;

	return us;
}

// 默认格式化时间为yyyy-MM-dd HH:mm:ss
/*
	d短日期 M/d/yy
	D长日期 yyyy-MM-dd
	t短时间 mm:ss
	T长时间 HH:mm:ss
	f短全部 M/d/yy HH:mm
	F长全部 yyyy-MM-dd HH:mm:ss
*/
const char* DateTime::ToString(byte kind, string str)
{
	//assert_param(str);
	if(!str) str = _Str;

	const DateTime& st = *this;
	switch(kind)
	{
		case 'd':
			sprintf(str, "%d/%d/%02d", st.Month, st.Day, st.Year % 100);
			break;
		case 'D':
			sprintf(str, "%04d-%02d-%02d", st.Year, st.Month, st.Day);
			break;
		case 't':
			sprintf(str, "%02d:%02d", st.Hour, st.Minute);
			break;
		case 'T':
			sprintf(str, "%02d:%02d:%02d", st.Hour, st.Minute, st.Second);
			break;
		case 'f':
			sprintf(str, "%d/%d/%02d %02d:%02d", st.Month, st.Day, st.Year % 100, st.Hour, st.Minute);
			break;
		case 'F':
			sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d", st.Year, st.Month, st.Day, st.Hour, st.Minute, st.Second);
			break;
		default:
			assert_param(false);
			break;
	}

	return str;
}

// 当前时间
DateTime& TTime::Now()
{
	_Now.Parse(Current());

	return _Now;
}

TimeWheel::TimeWheel(uint seconds, uint ms, uint us)
{
	Sleep = 0;
	Reset(seconds, ms, us);
}

void TimeWheel::Reset(uint seconds, uint ms, uint us)
{
	Expire = ((seconds * 1000) + ms) * 1000 + us;
	Expire += Time.Current();
}

// 是否已过期
bool TimeWheel::Expired()
{
	if(Time.Current() >= Expire) return true;

	// 睡眠，释放CPU
	if(Sleep) Sys.Sleep(Sleep);

	return false;
}
