#include "Timer.h"

//static TIM_TypeDef* g_Timers[] = TIMS;

Timer::Timer(TIM_TypeDef* timer)
{
	assert_param(timer);

	TIM_TypeDef* g_Timers[] = TIMS;
	_index = 0xFF;
	for(int i=0; i<ArrayLength(g_Timers); i++)
	{
		if(g_Timers[i] == timer)
		{
			_index = i;
			break;
		}
	}
	assert_param(_index <= ArrayLength(g_Timers));

	_port = g_Timers[_index];

	// 默认情况下，预分频到1MHz，然后1000个周期，即是1ms中断一次
	/*Prescaler = Sys.Clock / 1000000;
	Period = 1000;*/
	SetFrequency(10);

	_started = false;

	_Handler = NULL;
	_Param = NULL;
}

Timer::~Timer()
{
	if(_started) Stop();

	if(_Handler) Register(NULL);
}

void Timer::Start()
{
#if DEBUG
    // 获取当前频率
    RCC_ClocksTypeDef clock;
    RCC_GetClocksFreq(&clock);

	uint clk = clock.PCLK1_Frequency;
	if((uint)_port & 0x00010000) clk = clock.PCLK2_Frequency;
	clk <<= 1;

	uint fre = clk / Prescaler / Period;
	debug_printf("Timer%d_Start Prescaler=%d Period=%d Frequency=%d\r\n", _index + 1, Prescaler, Period, fre);
#endif

	// 打开时钟
	ClockCmd(true);

	// 关闭
	TIM_DeInit(_port);

	// 配置时钟
	TIM_TimeBaseInitTypeDef _timer;
	TIM_TimeBaseStructInit(&_timer);
	_timer.TIM_Period = Period - 1;
	_timer.TIM_Prescaler = Prescaler - 1;
	//_timer.TIM_ClockDivision = 0x0;
	_timer.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(_port, &_timer);

	// 打开中断
	//TIM_ITConfig(_port, TIM_IT_Update | TIM_IT_Trigger, ENABLE);
	TIM_ITConfig(_port, TIM_IT_Update, ENABLE);
	//TIM_UpdateRequestConfig(_port, TIM_UpdateSource_Regular);
	//TIM_PrescalerConfig(_port, Prescaler - 1, TIM_PSCReloadMode_Update);

	// 打开计数
	TIM_Cmd(_port, ENABLE);

	_started = true;
}

void Timer::Stop()
{
	if(!_started) return;

	// 关闭时钟
	ClockCmd(false);
	TIM_ITConfig(_port, TIM_IT_Update, DISABLE);
	TIM_Cmd(_port, DISABLE);

	_started = false;
}

void Timer::ClockCmd(bool state)
{
	FunctionalState st = state ? ENABLE : DISABLE;
	switch(_index + 1)
	{
		case 1: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, st); break;
		case 2: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, st); break;
		case 3: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, st); break;
		case 4: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, st); break;
		case 5: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, st); break;
		case 6: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, st); break;
		case 7: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM7, st); break;
		case 8: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, st); break;
#ifdef STM32F4
		case 9: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, st); break;
		case 10: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM10, st); break;
		case 11: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM11, st); break;
		case 12: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM12, st); break;
		case 13: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM13, st); break;
		case 14: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM14, st); break;
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
    RCC_ClocksTypeDef clock;
    RCC_GetClocksFreq(&clock);

	uint clk = clock.PCLK1_Frequency;
	if((uint)_port & 0x00010000) clk = clock.PCLK2_Frequency;
	clk <<= 1;

	assert_param(frequency > 0 && frequency <= clk);

	uint s = 1;
	uint p = s / frequency;

    uint pre = clk / s; // prescaler

	while (pre >= 0x10000 || p == 0) { // prescaler 太大
		if (p >= 0x80000000) return;
		s *= 10;
		pre /= 10;
		p = s / frequency;
	}

    if (_index+1 != 2 && _index+1 != 5) { // 16 bit timer
        while (p >= 0x10000) { // period too large
            if (pre > 0x8000) return;
            pre <<= 1;
            p >>= 1;
        }
    }

	Prescaler = pre;
	Period = p;
}

void Timer::Register(EventHandler handler, void* param)
{
	_Handler = handler;
	_Param = param;

	int irqs[] = TIM_IRQns;
	if(handler)
		Interrupt.Activate(irqs[_index], OnHandler, this);
	else
		Interrupt.Deactivate(irqs[_index]);
}

void Timer::OnHandler(ushort num, void* param)
{
	Timer* timer = (Timer*)param;
	if(timer) timer->OnInterrupt();
}

void Timer::OnInterrupt()
{
	// 检查指定的 TIM 中断发生
	if(TIM_GetITStatus(_port, TIM_IT_Update) == RESET) return;
	// 必须清除TIMx的中断待处理位，否则会频繁中断
	TIM_ClearITPendingBit(_port, TIM_IT_Update);
	
	if(_Handler) _Handler(this, _Param);
}
