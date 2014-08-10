#include "Timer.h"

//static TIM_TypeDef* g_Timers[] = TIMS;

Timer::Timer(byte index)
{
	_index = index;
	TIM_TypeDef* g_Timers[] = TIMS;
	_port = g_Timers[index];
	TIM_TimeBaseStructInit(&_timer);

	// 默认情况下，预分频到1MHz，然后1000个周期，即是1ms中断一次
	Prescaler = Sys.Clock / 1000000;
	Period = 999;

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
	// 打开时钟
	switch(_index+1)
	{
		case 1: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE); break;
		case 2: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE); break;
		case 3: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); break;
		case 6: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE); break;
	}

	// 关闭
	TIM_DeInit(_port);

	// 配置时钟
	_timer.TIM_Period = Period - 1;
	_timer.TIM_Prescaler = Prescaler - 1;
	_timer.TIM_ClockDivision = 0x0;
	_timer.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(_port, &_timer);

	// 打开中断
	TIM_ITConfig(_port, TIM_IT_Update, ENABLE);

	// 打开计数
	TIM_Cmd(_port, ENABLE);

	_started = true;
}

void Timer::Stop()
{
	if(!_started) return;

	// 关闭时钟
	switch(_index+1)
	{
		case 1: RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, DISABLE); break;
		case 2: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE); break;
		case 3: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, DISABLE); break;
		case 6: RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, DISABLE); break;
	}
	TIM_ITConfig(_port, TIM_IT_Update, DISABLE);
	TIM_Cmd(_port, DISABLE);

	_started = false;
}

void Timer::Register(TimerHandler handler, void* param)
{
	_Handler = handler;
	_Param = param;

	int TIMER_IRQns[] = {
		TIM1_CC_IRQn, TIM2_IRQn, TIM3_IRQn,
#ifdef STM32F10X
		TIM4_IRQn, TIM5_IRQn, TIM6_DAC_IRQn
#endif
	};
	if(handler)
		Interrupt.Activate(TIMER_IRQns[_index], OnHandler, this);
	else
		Interrupt.Deactivate(TIMER_IRQns[_index]);
}

void Timer::OnHandler(ushort num, void* param)
{
	Timer* timer = (Timer*)param;
	if(timer && timer->_Handler) timer->_Handler(timer, timer->_Param);
}
