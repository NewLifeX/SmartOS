#include "Time.h"

#include "Platform\stm32.h"

#define TIME_DEBUG 0

// 截止2000-01-01的所有秒数
#define BASE_YEAR_US				63082281600UL

/************************************************ TTime ************************************************/

#define TIME_Completion_IdleValue 0xFFFFFFFFFFFFFFFFull

#define SYSTICK_MAXCOUNT       SysTick_LOAD_RELOAD_Msk	//((1<<24) - 1)	/* SysTick MaxCount */
#define SYSTICK_ENABLE         SysTick_CTRL_ENABLE_Msk	//     0		/* Config-Bit to start or stop the SysTick Timer */

static TIM_TypeDef* const g_Timers[] = TIMS;

TTime::TTime()
{
	Seconds	= 0;
	Ticks	= 0;
#if defined(STM32F0) || defined(GD32F150)
	Index	= 13;
#else
	Div		= 0;
	Index	= 5;
#endif
	BaseSeconds = 0;

	OnInit	= NULL;
	OnLoad	= NULL;
	OnSave	= NULL;
	OnSleep	= NULL;
}

void TTime::Init()
{
	// 准备使用外部时钟，Systick时钟=HCLK/8
	uint clk = Sys.Clock / 8;
	// 48M时，每秒48M/8=6M个滴答，1us=6滴答
	// 72M时，每秒72M/8=9M个滴答，1us=9滴答
	// 96M时，每秒96M/8=12M个滴答，1us=12滴答
	// 120M时，每秒120M/8=15M个滴答，1us=15滴答
	// 168M时，每秒168M/8=21M个滴答，1us=21滴答
	Ticks = clk / 1000000;	// 每微秒的时钟滴答数

	//uint ticks = SYSTICK_MAXCOUNT;
	// ticks为每次中断的嘀嗒数，也就是重载值
	//SysTick_Config(ticks);
	// 上面的函数会打开中断
	uint ticks = Ticks * 1000;	// 1000微秒，便于跟毫秒叠加
	SysTick->LOAD  = ticks - 1;
	SysTick->VAL   = 0;
	SysTick->CTRL  = SysTick_CTRL_ENABLE_Msk;
	//Interrupt.Disable(SysTick_IRQn);

	TIM_TypeDef* tim = g_Timers[Index];
#if defined(STM32F0) || defined(GD32F150)
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM14, ENABLE);
#else
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
#endif

    // 获取当前频率
#if defined(STM32F0) || defined(GD32F150)
	uint prd	= 1000;
	uint psc	= Sys.Clock / 1000;
#else
	clk = RCC_GetPCLK();
	if((uint)tim & 0x00010000) clk = RCC_GetPCLK2();
	clk <<= 1;

	// 120M时，分频系数必须是120K才能得到1k的时钟，超过了最大值64k
	// 因此，需要增加系数
	uint prd	= 1000;
	uint psc	= clk / 1000;
	Div = 0;
	while(psc > 0xFFFF)
	{
		prd	<<= 1;
		psc	>>= 1;
		Div++;
	}
#endif

	// 配置时钟。1毫秒计时，1000毫秒中断
	TIM_TimeBaseInitTypeDef tr;
	TIM_TimeBaseStructInit(&tr);
	tr.TIM_Period		= prd - 1;
	tr.TIM_Prescaler	= psc - 1;
	//tr.TIM_ClockDivision = 0x0;
	tr.TIM_CounterMode	= TIM_CounterMode_Up;
	TIM_TimeBaseInit(tim, &tr);

	// 打开中断
	TIM_ITConfig(tim, TIM_IT_Update, ENABLE);
	// 清除标志位  必须要有！！ 否则 开启中断立马中断给你看
	TIM_ClearFlag(tim, TIM_FLAG_Update);

	const int irqs[] = TIM_IRQns;
	Interrupt.SetPriority(irqs[Index], 0);
	Interrupt.Activate(irqs[Index], OnHandler, tim);

	// 打开计数
	TIM_Cmd(tim, ENABLE);
}

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

#if  defined(STM32F0) || defined(GD32F150) || defined(STM32F4)
    #define SysTick_CTRL_COUNTFLAG SysTick_CTRL_COUNTFLAG_Msk
#endif
void TTime::OnHandler(ushort num, void* param)
{
	auto timer = (TIM_TypeDef*)param;
	if(!timer) return;

	// 检查指定的 TIM 中断发生
	if(TIM_GetITStatus(timer, TIM_IT_Update) == RESET) return;

	// 累加计数
	auto& time	= (TTime&)Time;
	time.Seconds += 1;
	time.Milliseconds += 1000;
	// 必须清除TIMx的中断待处理位，否则会频繁中断
	TIM_ClearITPendingBit(timer, TIM_IT_Update);
	//debug_printf("TTime::OnHandler Seconds=%d Milliseconds=%d\r\n", Time.Seconds, (uint)Time.Milliseconds);

	// 定期保存Ticks到后备RTC寄存器
	if(time.OnSave) time.OnSave();

	//if(Sys.OnTick) Sys.OnTick();
}

// 当前滴答时钟
uint TTime::CurrentTicks() const
{
	return SysTick->LOAD - SysTick->VAL;
}

// 当前毫秒数
ulong TTime::Current() const
{
	uint cnt = g_Timers[Index]->CNT;
#if ! (defined(STM32F0) || defined(GD32F150))
	if(Div) cnt >>= Div;
#endif
	return Milliseconds + cnt;
}

void TTime::SetTime(ulong seconds)
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

#pragma arm section code

// 当前时间
DateTime TTime::Now() const
{
	DateTime dt(Seconds + BaseSeconds);
	//dt.Millisecond = Milliseconds;
	return dt;
}

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
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
    if(!ms || running != NULL && !*running) return;

	uint me	= Current() + ms;

    while(true)
	{
		if(Current() >= me) break;

		if(running != NULL && !*running) break;
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
	ulong ms	= Current();
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

#pragma arm section code

/************************************************ DateTime ************************************************/

/// 我们的时间起点是 1/1/2000 00:00:00.000.000 在公历里面1/1/2000是星期六
#define BASE_YEAR                   2000
#define BASE_YEAR_LEAPYEAR_ADJUST   484
#define BASE_YEAR_DAYOFWEEK_SHIFT   6		// 星期偏移
//#define BASE_YEAR_US				63082281600UL

const int CummulativeDaysForMonth[13] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};

#define IS_LEAP_YEAR(y)             (((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0))
#define NUMBER_OF_LEAP_YEARS(y)     ((((y - 1) / 4) - ((y - 1) / 100) + ((y - 1) / 400)) - BASE_YEAR_LEAPYEAR_ADJUST) // 基于基本年1601的闰年数，不包含当年
#define NUMBER_OF_YEARS(y)          (y - BASE_YEAR)

#define YEARS_TO_DAYS(y)            ((NUMBER_OF_YEARS(y) * 365) + NUMBER_OF_LEAP_YEARS(y))
#define MONTH_TO_DAYS(y, m)         (CummulativeDaysForMonth[m - 1] + ((IS_LEAP_YEAR(y) && (m > 2)) ? 1 : 0))

DateTime& DateTime::Parse(ulong seconds)
{
	DateTime& st = *this;

	if(seconds >= BASE_YEAR_US) seconds -= BASE_YEAR_US;

	// 分别计算毫秒、秒、分、时，剩下天数
	uint time = seconds;
    st.Second = time % 60;
    time /= 60;
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

DateTime& DateTime::ParseUs(ulong us)
{
	Parse(us / 1000000ULL);
	uint n = us % 1000000ULL;
	Millisecond	= n / 1000;
	Microsecond	= n % 1000;

	return *this;
}

DateTime::DateTime()
{
	memset(&Year, 0, &Microsecond - &Year + sizeof(Microsecond));
}

DateTime::DateTime(ulong seconds)
{
	if(seconds == 0)
		memset(&Year, 0, &Microsecond - &Year + sizeof(Microsecond));
	else
		Parse(seconds);
}

// 重载等号运算符
DateTime& DateTime::operator=(ulong seconds)
{
	Parse(seconds);

	return *this;
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
	ulong sec = (ulong)TotalSeconds();
	uint us = (uint)Millisecond * 1000 + Microsecond;

	return sec * 1000 + us;
}

String& DateTime::ToStr(String& str) const
{
	// F长全部 yyyy-MM-dd HH:mm:ss
	/*str.Concat(Year, 10, 4);
	str	+= '-';
	str.Concat(Month, 10, 2);
	str	+= '-';
	str.Concat(Day, 10, 2);
	str	+= ' ';
	str.Concat(Hour, 10, 2);
	str	+= ':';
	str.Concat(Minute, 10, 2);
	str	+= ':';
	str.Concat(Second, 10, 2);*/

	char cs[20];
	sprintf(cs, "%04d-%02d-%02d %02d:%02d:%02d", Year, Month, Day, Hour, Minute, Second);
	str	+= cs;

	return str;
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
const char* DateTime::GetString(byte kind, char* str)
{
	assert_param(str);
	//if(!str) str = _Str;

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
	ulong now = Time.Current();
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

void TimeCost::Show(const char* format)
{
	if(!format) format = "执行 %dus\r\n";
	debug_printf(format, Elapsed());
}
