#include "Kernel\Sys.h"
#include "Kernel\Interrupt.h"
#include "Kernel\TTime.h"
#include "Device\Timer.h"

#include "Platform\stm32.h"

#define TIME_DEBUG 0

/************************************************ TTime ************************************************/

#define SYSTICK_MAXCOUNT       SysTick_LOAD_RELOAD_Msk	//((1<<24) - 1)	/* SysTick MaxCount */
#define SYSTICK_ENABLE         SysTick_CTRL_ENABLE_Msk	//     0		/* Config-Bit to start or stop the SysTick Timer */

static TIM_TypeDef* const g_Timers[] = TIMS;
static uint	gTicks;

void TTime::Init()
{
	// 准备使用外部时钟，Systick时钟=HCLK/8
	uint clk = Sys.Clock / 8;
	// 48M时，每秒48M/8=6M个滴答，1us=6滴答
	// 72M时，每秒72M/8=9M个滴答，1us=9滴答
	// 96M时，每秒96M/8=12M个滴答，1us=12滴答
	// 120M时，每秒120M/8=15M个滴答，1us=15滴答
	// 168M时，每秒168M/8=21M个滴答，1us=21滴答
	gTicks = clk / 1000000;	// 每微秒的时钟滴答数

	//uint ticks = SYSTICK_MAXCOUNT;
	// ticks为每次中断的嘀嗒数，也就是重载值
	//SysTick_Config(ticks);
	// 上面的函数会打开中断
	uint ticks = gTicks * 1000;	// 1000微秒，便于跟毫秒叠加
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

	const byte irqs[] = TIM_IRQns;
	byte irq	= irqs[Index];
	Interrupt.SetPriority(irq, 0);
	Interrupt.Activate(irq, OnHandler, tim);

	// 打开计数
	TIM_Cmd(tim, ENABLE);
}

#if  defined(STM32F0) || defined(GD32F150) || defined(STM32F4)
    #define SysTick_CTRL_COUNTFLAG SysTick_CTRL_COUNTFLAG_Msk
#endif
// 关键性代码，放到开头
INROOT void TTime::OnHandler(ushort num, void* param)
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
INROOT uint TTime::CurrentTicks() const
{
	return SysTick->LOAD - SysTick->VAL;
}

// 当前毫秒数
INROOT UInt64 TTime::Current() const
{
	uint cnt = g_Timers[Index]->CNT;
#if ! (defined(STM32F0) || defined(GD32F150))
	if(Div) cnt >>= Div;
#endif
	return Milliseconds + cnt;
}

INROOT uint TTime::TicksToUs(uint ticks) const	{ return !ticks ? 0 : (ticks / gTicks); }
INROOT uint TTime::UsToTicks(uint us) const	{ return !us ? 0 : (us * gTicks); }
