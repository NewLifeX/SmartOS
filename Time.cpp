#include "Time.h"
#include "Timer.h"

#include "Platform\stm32.h"

#define TIME_DEBUG 0

#define UTC		true								// utc 从1970/1/1 开始计时
#define UTC_CALIBRATE				946684800UL		// 2000/1/1 - 1970/1/1 秒值
#ifdef UTC
#define BASE_YEAR_US				62135596800UL		// (63082281600UL-UTC_CALIBRATE)	// 从0 到 2000-01-01的所有秒数
#else
#define BASE_YEAR_US				63082281600UL	// 从0 到 2000-01-01的所有秒数
#endif
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
/*#if defined(STM32F0) || defined(GD32F150)
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM14, ENABLE);
#else
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);
#endif*/
	Timer::ClockCmd(Index, true);

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
UInt64 TTime::Current() const
{
	uint cnt = g_Timers[Index]->CNT;
#if ! (defined(STM32F0) || defined(GD32F150))
	if(Div) cnt >>= Div;
#endif
	return Milliseconds + cnt;
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

#pragma arm section code

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
    if(!ms || running != nullptr && !*running) return;

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

#pragma arm section code

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
