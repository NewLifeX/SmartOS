#include "Sys.h"
#include "Kernel\Interrupt.h"
#include "Device\Timer.h"

#include "Platform\stm32.h"

static TIM_TypeDef* const g_Timers[] = TIMS;
const byte Timer::TimerCount = ArrayLength(g_Timers);

const void* Timer::GetTimer(byte idx)
{
	return g_Timers[idx];
}

void Timer::OnInit()
{
	assert_param(_index <= ArrayLength(g_Timers));

	_Timer		= g_Timers[_index];
}

void Timer::Config()
{
	TS("Timer::Config");

	auto ti	= (TIM_TypeDef*)_Timer;

	// 配置时钟
	TIM_TimeBaseInitTypeDef tr;
	TIM_TimeBaseStructInit(&tr);
	tr.TIM_Period		= Period - 1;
	tr.TIM_Prescaler	= Prescaler - 1;
	//tr.TIM_ClockDivision = 0x0;
	tr.TIM_CounterMode	= TIM_CounterMode_Up;
	TIM_TimeBaseInit(ti, &tr);

	//TIM_PrescalerConfig(ti, tr.TIM_Period,TIM_PSCReloadMode_Immediate);		// 分频数立即加载
	// 打开中断
	//TIM_ITConfig(ti, TIM_IT_Update | TIM_IT_Trigger, ENABLE);
	TIM_ITConfig(ti, TIM_IT_Update, ENABLE);
	//TIM_UpdateRequestConfig(ti, TIM_UpdateSource_Regular);
	// 清除标志位  必须要有！！ 否则 开启中断立马中断给你看
	TIM_ClearFlag(ti, TIM_FLAG_Update);
	//TIM_ClearITPendingBit(ti, TIM_IT_Update);
}

void Timer::OnOpen()
{
#if DEBUG
    // 获取当前频率
	uint clk = RCC_GetPCLK();
#if defined(STM32F1) || defined(STM32F4)
	if((uint)_Timer & 0x00010000) clk = RCC_GetPCLK2();
#endif
	if(clk < Sys.Clock) clk <<= 1;

	uint fre = clk / Prescaler / Period;
	debug_printf("Timer%d::Open clk=%d Prescaler=%d Period=%d Fre=%d\r\n", _index + 1, clk, Prescaler, Period, fre);
	assert(fre > 0, "频率超出范围");
#endif

	// 打开时钟
	ClockCmd(_index, true);

	// 关闭。不再需要，跟上面ClockCmd的效果一样
	//TIM_DeInit((TIM_TypeDef*)_Timer);

	Config();

	// 打开计数
	TIM_Cmd((TIM_TypeDef*)_Timer, ENABLE);
}

void Timer::OnClose()
{
	auto ti	= (TIM_TypeDef*)_Timer;
	// 关闭计数器时钟
	TIM_Cmd(ti, DISABLE);
	TIM_ITConfig(ti, TIM_IT_Update, DISABLE);
	TIM_ClearITPendingBit(ti, TIM_IT_Update);	// 仅清除中断标志位 关闭不可靠
	ClockCmd(_index, false);	// 关闭定时器时钟
}

void Timer::ClockCmd(int idx, bool state)
{
	FunctionalState st = state ? ENABLE : DISABLE;
	switch(idx + 1)
	{
		case 1: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, st); break;
		case 2: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, st); break;
		case 3: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, st); break;
#if defined(STM32F1) && defined(STM32F4)
		case 4: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, st); break;
		case 5: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, st); break;
#endif
		case 6: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, st); break;
#if defined(STM32F1) && defined(STM32F4)
		case 7: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, st); break;
		case 8: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, st); break;
#endif
#ifdef STM32F4
		case 9: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, st); break;
		case 10: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, st); break;
		case 11: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM11, st); break;
		case 12: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM12, st); break;
		case 13: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM13, st); break;
		case 14: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM14, st); break;
#endif
#if defined(STM32F0) || defined(GD32F150)
		case 14: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM14, st); break;
		case 15: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM15, st); break;
		case 16: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM16, st); break;
		case 17: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM17, st); break;
#endif
	}
}

// 设置预分频目标，比如1MHz
/*void Timer::SetScaler(uint scaler)
{
	assert_param(scaler);

	uint ps = Sys.Clock / scaler;
	assert_param(ps > 0 && ps <= 0xFFFF);
}*/

// 设置频率，自动计算预分频
void Timer::SetFrequency(uint frequency)
{
    // 获取当前频率
#if defined(STM32F0) || defined(GD32F150)
	uint clk	= Sys.Clock;
#else
	uint clk = RCC_GetPCLK();
	if((uint)_Timer & 0x00010000) clk = RCC_GetPCLK2();
	clk <<= 1;
#endif

	// 120M时，分频系数必须是120K才能得到1k的时钟，超过了最大值64k
	// 因此，需要增加系数
	uint prd	= clk / frequency;
	uint psc	= 1;
	uint Div	= 0;
	while(prd > 0xFFFF)
	{
		prd	>>= 1;
		psc	<<= 1;
		Div++;
	}

	assert(frequency > 0 && frequency <= clk, "频率超出范围");

	Prescaler	= psc;
	Period		= prd;

	// 如果已启动定时器，则重新配置一下，让新设置生效
	if(Opened)
	{
		TIM_TimeBaseInitTypeDef tr;
		TIM_TimeBaseStructInit(&tr);
		tr.TIM_Period		= Period - 1;
		tr.TIM_Prescaler	= Prescaler - 1;
		//tr.TIM_ClockDivision = 0x0;
		tr.TIM_CounterMode = TIM_CounterMode_Up;
		TIM_TimeBaseInit((TIM_TypeDef*)_Timer, &tr);
	}
}

uint Timer::GetCounter()
{
	return TIM_GetCounter((TIM_TypeDef*)_Timer);
}

void Timer::SetCounter(uint cnt)
{
	TIM_SetCounter((TIM_TypeDef*)_Timer, cnt);
}

void Timer::SetHandler(bool set)
{
	const byte irqs[] = TIM_IRQns;
	byte irq	= irqs[_index];
	if(set)
	{
		// 打开中断
		//TIM_ITConfig((TIM_TypeDef*)_Timer, TIM_IT_Update, ENABLE);
		Interrupt.SetPriority(irq, 1);
		Interrupt.Activate(irq, OnHandler, this);
	}
	else
	{
		TIM_ITConfig((TIM_TypeDef*)_Timer, TIM_IT_Update, DISABLE);
		Interrupt.Deactivate(irq);
	}
}

void Timer::OnHandler(ushort num, void* param)
{
	auto timer = (Timer*)param;
	if(timer)
	{
		auto ti	= (TIM_TypeDef*)timer->_Timer;
		// 检查指定的 TIM 中断发生
		if(TIM_GetITStatus(ti, TIM_IT_Update) == RESET) return;
		// 必须清除TIMx的中断待处理位，否则会频繁中断
		TIM_ClearITPendingBit(ti, TIM_IT_Update);

		timer->OnInterrupt();
	}
}
