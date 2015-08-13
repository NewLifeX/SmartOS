#include "Time.h"

#define TIME_DEBUG 0

/************************************************ TTime ************************************************/

#define TIME_Completion_IdleValue 0xFFFFFFFFFFFFFFFFull

#define SYSTICK_MAXCOUNT       SysTick_LOAD_RELOAD_Msk	//((1<<24) - 1)	/* SysTick MaxCount */
#define SYSTICK_ENABLE         SysTick_CTRL_ENABLE_Msk	//     0		/* Config-Bit to start or stop the SysTick Timer */

TTime::TTime()
{
	Ticks			= 0;
	Microseconds	= 0;
	Milliseconds	= 0;
	//NextEvent = TIME_Completion_IdleValue;

	_RTC	= NULL;

	_usTicks= 0;
	_msUs	= 0;
}

TTime::~TTime()
{
    Interrupt.Deactivate(SysTick_IRQn);
    // 关闭定时器
	SysTick->CTRL &= ~SYSTICK_ENABLE;
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
	TicksPerMicrosecond = clk / 1000000;	// 每微秒的时钟滴答数

	if(_RTC) _RTC->Init();

	SetMax(0);

	// 必须放在SysTick_Config后面，因为它设为不除以8
	//SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK);
	// 本身主频已经非常快，除以8分频吧
	SysTick_CLKSourceConfig(SysTick_CLKSource_HCLK_Div8);

	// 需要最高优先级
    Interrupt.SetPriority(SysTick_IRQn, 0);
	Interrupt.Activate(SysTick_IRQn, OnHandler, this);
}

// 设置最大计数，也就是滴答定时器重载值
void TTime::SetMax(uint usMax)
{
	/*SysTick->CTRL &= ~SysTick_CTRL_CLKSOURCE_Msk;	// 选择外部时钟，每秒有个HCLK/8滴答
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;		// 开启定时器减到0后的中断请求

	// 加载嘀嗒数，72M时~=0x00FFFFFF/9M=1864135us，96M时~=0x00FFFFFF/12M=1398101us
	SysTick->LOAD = SYSTICK_MAXCOUNT - 1;
	SysTick->VAL = 0;
	SysTick->CTRL |= SYSTICK_ENABLE;	// SysTick使能*/

	// InterruptsPerSecond单位：中断每秒，clk单位：滴答每秒，ticks单位：滴答每中断
	// 默认100，也即是每秒100次中断，10ms一次
	uint ticks = SYSTICK_MAXCOUNT;
	if(usMax > 0) ticks = usMax * TicksPerMicrosecond;
	// ticks为每次中断的嘀嗒数，也就是重载值
	assert_param(ticks > 0 && ticks <= SYSTICK_MAXCOUNT);
	SysTick_Config(ticks);
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

		/*Time._usTicks += value;
		uint us = Time._usTicks / Time.TicksPerMicrosecond;
		Time.Microseconds += us;
		Time._usTicks %= Time.TicksPerMicrosecond;

		Time._msUs += us;
		Time.Milliseconds += Time._msUs / 1000;
		Time._msUs %= 1000;*/

		// 这次的嘀嗒数加上上一次的剩余量，折算为微秒数
		volatile uint ts = Time._usTicks + value;
		uint us = ts / Time.TicksPerMicrosecond;

		// 这次的微秒数加上上一次的剩余量
		volatile uint ms = Time._msUs + us;

		// 从大到小累加单位，避免出现总时间变小的情况
		Time.Milliseconds += ms / 1000;
		Time.Microseconds += us;
		Time._msUs = ms % 1000;
		Time._usTicks = ts % Time.TicksPerMicrosecond;
	}
	// 定期保存Ticks到后备RTC寄存器
	if(Time._RTC) Time._RTC->SaveTicks();

	if(Sys.OnTick) Sys.OnTick();
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

	//return (Ticks + value) / TicksPerMicrosecond;
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

	// 保存到RTC
	if(_RTC) _RTC->SaveTicks();
}

#define STM32_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS 3

void TTime::Sleep(uint us, bool* running)
{
    // 睡眠时间太短
    if(us <= STM32_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS) return ;

	// 较大的睡眠时间直接让CPU停下来
	if(_RTC && _RTC->LowPower && us >= 10000)
	{
		uint ms = us / 1000;
		_RTC->Sleep(ms);
		// CPU睡眠是秒级，还有剩余量
		us %= 1000;
		us += ms * 1000;
	}
    // 睡眠时间太短
    if(us <= STM32_SLEEP_USEC_FIXED_OVERHEAD_CLOCKS || !*running) return ;

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

    while(CurrentTicks() <= maxDiff && (running == NULL || *running));
    //while(Current() <= maxDiff);
}

/************************************************ DateTime ************************************************/

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

DateTime::DateTime(ulong us)
{
	if(us == 0)
		memset(&Year, 0, &Microsecond - &Year + sizeof(Microsecond));
	else
		Parse(us);
}

// 重载等号运算符
DateTime& DateTime::operator=(ulong v)
{
	Parse(v);

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
	str.Append(Year, 10, 4).Append('-');
	str.Append(Month, 10, 2).Append('-');
	str.Append(Day, 10, 2).Append(' ');
	str.Append(Hour, 10, 2).Append(':');
	str.Append(Minute, 10, 2).Append(':');
	str.Append(Second, 10, 2);

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
const char* DateTime::GetString(byte kind, string str)
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

// 当前时间
DateTime TTime::Now()
{
	/*DateTime _Now;

	_Now.Parse(Current());

	return _Now;*/

	return DateTime(Current());
}

/************************************************ TimeWheel ************************************************/

TimeWheel::TimeWheel(uint seconds, uint ms, uint us)
{
	Sleep = 0;
	Reset(seconds, ms, us);
}

void TimeWheel::Reset(uint seconds, uint ms, uint us)
{
	Expire = ((seconds * 1000) + ms) * 1000 + us;
	Expire *= Time.TicksPerMicrosecond;
	Expire += Time.CurrentTicks();
}

// 是否已过期
bool TimeWheel::Expired()
{
	if(Time.CurrentTicks() >= Expire) return true;

	// 睡眠，释放CPU
	if(Sleep) Sys.Sleep(Sleep);

	return false;
}

TimeCost::TimeCost()
{
	Start = Time.CurrentTicks();
}

// 逝去的时间，微秒
int TimeCost::Elapsed()
{
	return ((int)(Time.CurrentTicks() - Start)) / Time.TicksPerMicrosecond;
}

void TimeCost::Show(const char* format)
{
	if(!format) format = "执行 %dus";
	debug_printf(format, Elapsed());
}

/************************************************ HardRTC ************************************************/

#ifdef STM32F1
void RTC_Configuration(bool init, bool ext)
{
	/* 备份寄存器模块复位，将BKP的全部寄存器重设为缺省值 */
	if(init) BKP_DeInit();

	if(!ext)
	{
		/* Enable LSI */
		RCC_LSICmd(ENABLE);
		/* 等待稳定 */
		while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY) == RESET);

		// 使用内部 32 KHz 晶振作为 RTC 时钟源
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
	}
	else
	{
		/* 设置外部低速晶振(LSE)32.768K  参数指定LSE的状态，可以是：RCC_LSE_ON：LSE晶振ON */
		RCC_LSEConfig(RCC_LSE_ON);
		/* 等待稳定 */
		while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);

		// RTC时钟源配置成LSE（外部32.768K）
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
	}

	/* 使能RTC时钟 */
	RCC_RTCCLKCmd(ENABLE);

	/* 开启后需要等待APB1时钟与RTC时钟同步，才能读写寄存器 */
	RTC_WaitForSynchro();

	/* 每一次读写寄存器前，要确定上一个操作已经结束 */
	RTC_WaitForLastTask();

	/* 使能秒中断 */
	//RTC_ITConfig(RTC_IT_SEC, ENABLE);

	/* 每一次读写寄存器前，要确定上一个操作已经结束 */
	//RTC_WaitForLastTask();

	if(init)
	{
		/* 设置RTC分频器，使RTC时钟为1Hz */
		//if(!ext)
			//RTC_SetPrescaler(31999); /* RTC period = RTCCLK/RTC_PR = (32.000 KHz)/(31999+1) */
		//else
			//RTC_SetPrescaler(32767); /* RTC period = RTCCLK/RTC_PR = (32.768 KHz)/(32767+1) */

		RTC_SetPrescaler(32);	// 为了使用低功耗，时钟为1kHz

		/* 每一次读写寄存器前，要确定上一个操作已经结束 */
		RTC_WaitForLastTask();
	}
}
#else
void RTC_Configuration(bool init, bool ext)
{
	RTC_WriteProtectionCmd(DISABLE);

	RTC_EnterInitMode();

	RTC_InitTypeDef rtc;
	RTC_StructInit(&rtc);
	rtc.RTC_HourFormat = RTC_HourFormat_24;
	rtc.RTC_AsynchPrediv = 0x7D-1;
	rtc.RTC_SynchPrediv = 0xFF-1;
	RTC_Init(&rtc);

	RTC_ExitInitMode();
	RTC_WriteProtectionCmd(ENABLE);	//初始化完成，设置标志
}

DateTime RTC_GetCounter()
{
	DateTime dt;

	RTC_TimeTypeDef time;
	RTC_GetTime(RTC_Format_BCD, &time);

	dt.Hour		= time.RTC_Hours;
	dt.Minute	= time.RTC_Minutes;
	dt.Second	= time.RTC_Seconds;

	RTC_DateTypeDef date;
	RTC_GetDate(RTC_Format_BCD, &date);

	dt.Year		= date.RTC_Year;
	dt.Month	= date.RTC_Month;
	dt.Day		= date.RTC_Date;

	return dt;
}

void RTC_SetCounter(DateTime& dt)
{
	RTC_TimeTypeDef time;
	RTC_TimeStructInit(&time);
	time.RTC_Seconds = dt.Second;
	time.RTC_Minutes = dt.Minute;
	time.RTC_Hours = dt.Hour;
	time.RTC_H12 = RTC_H12_AM;
	RTC_SetTime(RTC_Format_BCD, &time);

	RTC_DateTypeDef date;
	RTC_DateStructInit(&date);
	date.RTC_Date = dt.Day;
	date.RTC_Month = dt.Month;
	date.RTC_WeekDay= dt.DayOfWeek > 0 ? dt.DayOfWeek : RTC_Weekday_Sunday;
	date.RTC_Year = dt.Year;
	RTC_SetDate(RTC_Format_BCD, &date);
}

void RTC_WaitForLastTask()
{
	while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
}
#endif

static ulong g_NextSaveTicks;	// 下一次保存Ticks的数值，避免频繁保存
#if TIME_DEBUG
	static ulong g_Counter = 0;
#endif

void AlarmHandler(ushort num, void* param);

HardRTC::HardRTC(bool lowpower, bool external)
{
	LowPower	= lowpower;
	External	= external;
	Opened		= false;
}

void HardRTC::Init()
{
	/*****************************/
	/* GD32和STM32最大的区别就是STM32给寄存器默认值，而GD32没有给默认值 */
	/* 虽然RTC靠电池工作，断电后能保持配置，但是在GD32还是得重新打开时钟 */

	/* 启用PWR和BKP的时钟 */
#ifdef STM32F1
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
#else
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
#endif

	/* 后备域解锁 */
	PWR_BackupAccessCmd(ENABLE);

	// 下一次保存Ticks的数值，避免频繁保存
	g_NextSaveTicks = 0;

	Opened = true;

	if(ReadBackup(0) != 0xABCD)
	{
		RTC_Configuration(true, External);

		WriteBackup(0, 0xABCD);
	}
	else
	{
		//虽然RTC模块不需要重新配置，且掉电后依靠后备电池依然运行
		//但是每次上电后，还是要使能RTCCLK
		RTC_Configuration(false, External);

		// 从RTC还原滴答
		LoadTicks();
	}

	if(LowPower)
	{
		EXTI_ClearITPendingBit(EXTI_Line17);
		EXTI_InitTypeDef  exit;
		exit.EXTI_Line = EXTI_Line17;
		exit.EXTI_Mode = EXTI_Mode_Interrupt;
		exit.EXTI_Trigger = EXTI_Trigger_Rising;
		exit.EXTI_LineCmd = ENABLE;
		EXTI_Init(&exit);

#ifdef STM32F1
		Interrupt.SetPriority(RTCAlarm_IRQn, 0);
		Interrupt.Activate(RTCAlarm_IRQn, AlarmHandler, this);
#endif
	}
}

#ifndef STM32F1
	#define RTC_IT_ALR RTC_IT_ALRA
#endif

void HardRTC::LoadTicks()
{
	if(!Opened) return;

#ifdef STM32F1
	// 加上计数器的值，注意计数器的单位是秒。注意必须转INT64，否则溢出
	uint counter = RTC_GetCounter();
	// 计数器调整为毫秒，采用第二个后备寄存器保存秒以上的数据
	uint sec = ReadBackup(1);
	ulong ms = (ulong)sec * 1000 + counter;
#if TIME_DEBUG
	g_Counter = ms;
#endif
#else
	DateTime dt = RTC_GetCounter();
	ulong ms = dt.TotalMicroseconds();
#endif
	ulong us = ms * 1000;
	Time.Ticks = us * Time.TicksPerMicrosecond;
	Time.Milliseconds = ms;
	Time.Microseconds = us;
	//Time._msUs = 0;
	//Time._usTicks = 0;
}

void HardRTC::SaveTicks()
{
	if(!Opened) return;

	if(Time.Ticks < g_NextSaveTicks) return;
#if TIME_DEBUG
	if(g_Counter > 0)
	{
		debug_printf("LoadTicks %llu\r\n", g_Counter);

		DateTime dt(g_Counter * 1000ull);
		debug_printf("LoadTime: ");
		dt.Show(true);

		g_Counter = 0;
	}
#endif

	/* 等待最近一次对RTC寄存器的写操作完成，也即等待RTC寄存器同步 */
	RTC_WaitForSynchro();

#ifdef STM32F1
	ulong ms = Time.Current() / 1000;
	uint sec = (uint)(ms / 1000);
	uint ms2 = (uint)(ms % 1000);
#if TIME_DEBUG
	debug_printf("SaveTicks %ds %dms\r\n", sec, ms2);
#endif
	WriteBackup(1, sec);
	// 设置计数器
	RTC_SetCounter(ms2);
#else
	RTC_SetCounter(Time.Now());
#endif

	// 必须打开时钟和后备域，否则写不进去时间
	// Check if the Power On Reset flag is set
	//RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);

    /* Allow access to BKP Domain */
    //PWR_BackupAccessCmd(ENABLE);

	// 这里对时间要求非常苛刻，不能等待
	RTC_WaitForLastTask();

	// 每秒钟保存一次
	g_NextSaveTicks = Time.Ticks + 5000000ull * Time.TicksPerMicrosecond;
}

// 暂停系统一段时间
void HardRTC::Sleep(uint& ms)
{
	if(!Opened) return;

	if(ms <= 0) return;
	//int second = ms / 1000;
	//if(second <= 0) return;

#if TIME_DEBUG
	debug_printf("进入低功耗模式 %d 毫秒\r\n", ms);
#endif
	SaveTicks();

	// 打开RTC报警中断
	RTC_ITConfig(RTC_IT_ALR, ENABLE);
    // 设定报警时间
#ifdef STM32F1
    RTC_SetAlarm(RTC_GetCounter() + ms);
	//RTC_SetCounter(0);
	//RTC_SetAlarm(ms);
#else
	RTC_AlarmTypeDef alr;
	RTC_AlarmStructInit(&alr);

	int second = ms / 1000;

	RTC_AlarmCmd( RTC_Alarm_A, DISABLE );     /* Disable the Alarm A */
	alr.RTC_AlarmTime.RTC_H12 = RTC_H12_AM;
	alr.RTC_AlarmTime.RTC_Hours = second / 60 / 60;
	alr.RTC_AlarmTime.RTC_Minutes = (second / 60) % 60;
	alr.RTC_AlarmTime.RTC_Seconds = second % 60;

	/* Set the Alarm A */
	alr.RTC_AlarmDateWeekDay = 0x31;
	alr.RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date;
	alr.RTC_AlarmMask = RTC_AlarmMask_DateWeekDay;

	RTC_SetAlarm( RTC_Format_BIN, RTC_Alarm_A, &alr ); /* Configure the RTC Alarm A register */

	RTC_ITConfig( RTC_IT_ALRA, ENABLE );    /* Enable the RTC Alarm A Interrupt */

	RTC_AlarmCmd( RTC_Alarm_A, ENABLE );    /* Enable the alarm  A */
#endif
    /* Wait until last write operation on RTC registers has finished */
    RTC_WaitForLastTask();

	// 进入低功耗模式
	PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

	ms = 0;

	//debug_printf("离开低功耗模式\r\n");
}

uint HardRTC::ReadBackup(byte addr)
{
	if(!Opened) return 0;

#ifdef STM32F1
	return BKP_ReadBackupRegister(BKP_DR1 + (addr << 2));
#else
	return RTC_ReadBackupRegister(RTC_BKP_DR0 + (addr << 2));
#endif
}

void HardRTC::WriteBackup(byte addr, uint value)
{
	if(!Opened) return;

#ifdef STM32F1
	BKP_WriteBackupRegister(BKP_DR1 + (addr << 2), value);
#else
	RTC_WriteBackupRegister(RTC_BKP_DR0 + (addr << 2), value);
#endif
}

// 从停止模式醒来后配置系统时钟，打开HSE/PLL，选择PLL作为系统时钟源
void SYSCLKConfig_STOP(void)
{
  /* Enable HSE */
  RCC_HSEConfig(RCC_HSE_ON);

  /* Wait till HSE is ready */
  if(RCC_WaitForHSEStartUp() == SUCCESS)
  {
#ifdef STM32F10X_CL
    /* Enable PLL2 */
    RCC_PLL2Cmd(ENABLE);

    /* Wait till PLL2 is ready */
    while(RCC_GetFlagStatus(RCC_FLAG_PLL2RDY) == RESET);
#endif

    /* Enable PLL */
    RCC_PLLCmd(ENABLE);

    /* Wait till PLL is ready */
    while(RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

    /* Select PLL as system clock source */
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);

    /* Wait till PLL is used as system clock source */
    while(RCC_GetSYSCLKSource() != 0x08);
  }
}

void AlarmHandler(ushort num, void* param)
{
    SmartIRQ irq;

	HardRTC* rtc = (HardRTC*)param;

	if(RTC_GetITStatus(RTC_IT_ALR) != RESET)
	{
		EXTI_ClearITPendingBit(EXTI_Line17);
		// 检查唤醒标志是否设置
		if(PWR_GetFlagStatus(PWR_FLAG_WU) != RESET) PWR_ClearFlag(PWR_FLAG_WU);
		RTC_WaitForLastTask();
		RTC_ClearITPendingBit(RTC_IT_ALR);
		RTC_WaitForLastTask();
	}
	SYSCLKConfig_STOP();
	rtc->LoadTicks();

	//debug_printf("离开低功耗模式\r\n");
}
