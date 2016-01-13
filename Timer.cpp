#include "Timer.h"

#include "Platform\stm32.h"

static TIM_TypeDef* const g_Timers[] = TIMS;
const byte Timer::TimerCount = ArrayLength(g_Timers);

// 已经实例化的定时器对象
static Timer* Timers[16] = {
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
	NULL, NULL, NULL, NULL,
};

Timer::Timer(TIMER index)
{
	assert_param(index <= ArrayLength(g_Timers));

	Timers[index]	= this;

	_index		= index;
	_Timer		= g_Timers[index];

	// 默认情况下，预分频到1MHz，然后1000个周期，即是1ms中断一次
	/*Prescaler = Sys.Clock / 1000000;
	Period = 1000;*/
	SetFrequency(10);

	Opened	= false;

	_Handler	= NULL;
	_Param		= NULL;
}

Timer::~Timer()
{
	Close();

	if(_Handler) Register(NULL);

	Timers[_index] = NULL;
}

// 创建指定索引的定时器，如果已有则直接返回，默认0xFF表示随机分配
Timer* Timer::Create(byte index)
{
	TS("Timer::Create");

	byte tcount	= ArrayLength(g_Timers);
	// 特殊处理随机分配
	if(index == 0xFF)
	{
		// 找到第一个可用的位置，没有被使用，并且该位置定时器存在
		byte i = 0;
		for(; i<tcount && (Timers[i] || !g_Timers[i]); i++);

		if(i >= tcount)
		{
			debug_printf("Timer::Create 失败！没有空闲定时器可用！\r\n");
			return NULL;
		}

		index = i;
	}

	assert_param(index < tcount);

	if(Timers[index])
		return Timers[index];
	else
		return new Timer((TIMER)index);
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

void Timer::Open()
{
	if(Opened) return;

	TS("Timer::Open");

#if DEBUG
    // 获取当前频率
	uint clk = RCC_GetPCLK();
#if defined(STM32F1) || defined(STM32F4)
	if((uint)_Timer & 0x00010000) clk = RCC_GetPCLK2();
#endif
	if(clk < Sys.Clock) clk <<= 1;

	uint fre = clk / Prescaler / Period;
	debug_printf("Timer%d::Open clk=%d Prescaler=%d Period=%d Frequency=%d\r\n", _index + 1, clk, Prescaler, Period, fre);
#endif

	assert_param2(fre > 0, "频率超出允许的范围");

	// 打开时钟
	ClockCmd(true);

	// 关闭。不再需要，跟上面ClockCmd的效果一样
	//TIM_DeInit((TIM_TypeDef*)_Timer);

	Config();

	// 打开计数
	TIM_Cmd((TIM_TypeDef*)_Timer, ENABLE);

	Opened = true;
}

void Timer::Close()
{
	if(!Opened) return;

	TS("Timer::Close");

	debug_printf("Timer%d::Close\r\n", _index + 1);

	auto ti	= (TIM_TypeDef*)_Timer;
	// 关闭计数器时钟
	TIM_Cmd(ti, DISABLE);
	TIM_ITConfig(ti, TIM_IT_Update, DISABLE);
	TIM_ClearITPendingBit(ti, TIM_IT_Update);	// 仅清除中断标志位 关闭不可靠
	ClockCmd(false);	// 关闭定时器时钟

	Opened = false;
}

void Timer::ClockCmd(bool state)
{
	FunctionalState st = state ? ENABLE : DISABLE;
	switch(_index + 1)
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

	assert_param2(frequency > 0 && frequency <= clk, "频率超出允许的范围");

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
	int irqs[] = TIM_IRQns;
	if(set)
	{
		// 打开中断
		//TIM_ITConfig((TIM_TypeDef*)_Timer, TIM_IT_Update, ENABLE);
		Interrupt.SetPriority(irqs[_index], 1);
		Interrupt.Activate(irqs[_index], OnHandler, this);
	}
	else
	{
		TIM_ITConfig((TIM_TypeDef*)_Timer, TIM_IT_Update, DISABLE);
		Interrupt.Deactivate(irqs[_index]);
	}
}

void Timer::Register(EventHandler handler, void* param)
{
	_Handler	= handler;
	_Param		= param;

	SetHandler(handler != NULL);
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

void Timer::OnInterrupt()
{
	if(_Handler) _Handler(this, _Param);
}

/*================ PWM ================*/

// STM32F030 的   先这么写着 后面再对 103 做调整
typedef void (*TIM_OCInit)(TIM_TypeDef* TIMx, TIM_OCInitTypeDef* TIM_OCInitStruct);
const static TIM_OCInit OCInits[4]={TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init};
// 外部初始化引脚 ？？  AFIO很头疼
/*     @arg GPIO_AF_0:TIM15, TIM17, TIM14
 *     @arg GPIO_AF_1:Tim3, TIM15
 *     @arg GPIO_AF_2:TIM2, TIM1, TIM16, TIM17.
 *     @arg GPIO_AF_3:TIM15,
 *     @arg GPIO_AF_4:TIM14.
 *     @arg GPIO_AF_5:TIM16, TIM17.
 *     @arg GPIO_AF_6:
 *     @arg GPIO_AF_7:*/
//const static uint8_t TIM_CH_AFa[8]=
//{
//};

PWM::PWM(TIMER index) : Timer(index)
{
	for(int i=0; i<4; i++) Pulse[i] = 0xFFFF;

	Pulses		= NULL;
	PulseCount	= 0;
	Channel		= 0;
	PulseIndex	= 0xFF;
	Repeated	= false;
	Configed	= 0x00;
}

void PWM::Config()
{
	TS("PWM::Config");

	Timer::Config();	// 主要是配置时钟基础部分 TIM_TimeBaseInit

	TIM_OCInitTypeDef oc;

	TIM_OCStructInit(&oc);
	oc.TIM_OCMode		= TIM_OCMode_PWM1;
	oc.TIM_OutputState	= TIM_OutputState_Enable;
	oc.TIM_OCPolarity	= Polarity ? TIM_OCPolarity_High : TIM_OCPolarity_Low;
	oc.TIM_OCIdleState	= IdleState ? TIM_OCIdleState_Reset : TIM_OCIdleState_Set;

	auto ti	= (TIM_TypeDef*)_Timer;
	for(int i=0; i<4; i++)
	{
		if(Pulse[i] != 0xFFFF)
		{
			oc.TIM_Pulse = Pulse[i];
			OCInits[i](ti, &oc);
			Configed |= 0x1 << i;
		}
	}

	if(!Pulses)
	{
		// PWM模式用不上中断  直接就丢了吧  给中断管理减减麻烦
		TIM_ITConfig(ti, TIM_IT_Update, DISABLE);
		TIM_ClearFlag(ti, TIM_FLAG_Update);
	}

	TIM_SetCounter(ti, 0x00000000);		// 清零定时器CNT计数寄存器
	TIM_ARRPreloadConfig(ti, ENABLE);	// 使能预装载寄存器ARR

	// 如果需要连续调整宽度，那么需要中断
	if(Pulses) SetHandler(true);
}

void PWM::FlushOut()
{
	TIM_OCInitTypeDef oc;

	TIM_OCStructInit(&oc);
	oc.TIM_OCMode		= TIM_OCMode_PWM1;
	oc.TIM_OutputState	= TIM_OutputState_Enable;
	oc.TIM_OCPolarity	= Polarity ? TIM_OCPolarity_High : TIM_OCPolarity_Low;
	oc.TIM_OCIdleState	= IdleState ? TIM_OCIdleState_Reset : TIM_OCIdleState_Set;

	for(int i=0; i<4; i++)
	{
		if(Pulse[i] != 0xFFFF)
		{
			oc.TIM_Pulse = Pulse[i];
			OCInits[i]((TIM_TypeDef*)_Timer, &oc);
			Configed |= 0x1 << i;
		}
	}
}

void PWM::Open()
{
	Timer::Open();

#if defined(STM32F0) || defined(GD32F150)
	if(_index == 0 ||_index == 7||_index == 14 ||_index == 15|| _index == 16)
		TIM_CtrlPWMOutputs((TIM_TypeDef*)_Timer, ENABLE);
#endif

#if defined(STM32F1)
	if(_index == 0 ||_index == 14 ||_index == 15|| _index == 16)
		TIM_CtrlPWMOutputs((TIM_TypeDef*)_Timer, ENABLE);
#endif

#if	defined(STM32F4)
#endif
}

void PWM::Close()
{
#if defined(STM32F1) || defined(GD32F150)
	if(_index == 0 ||_index == 7||_index == 14 ||_index == 15|| _index == 16)
		TIM_CtrlPWMOutputs((TIM_TypeDef*)_Timer, DISABLE);
#elif defined(STM32F1)
	if(_index == 0 ||_index == 14 ||_index == 15|| _index == 16)
		TIM_CtrlPWMOutputs((TIM_TypeDef*)_Timer, DISABLE);
#else	//defined(STM32F4)
#endif

	Timer::Close();
}

void PWM::SetPulse(int idx, ushort pulse)
{
	if(Pulse[idx] == pulse) return;

	Pulse[idx] = pulse;

	if(Opened)
		Config();
	else
		Open();
}

void PWM::OnInterrupt()
{
	if(!Pulses || !PulseCount || PulseIndex > PulseCount) return;

	auto ti	= (TIM_TypeDef*)_Timer;
	// 动态计算4个寄存器中的某一个，并设置宽度
#ifdef STM32F1
	volatile ushort* reg = &(ti->CCR1);
#else
	volatile uint* reg = &(ti->CCR1);
#endif
	reg += Channel;

	// 发送完成以后，最后一次中断，把占空比调整为一半
	if(PulseIndex >= PulseCount)
	{
		PulseIndex++;
		*reg = Period >> 1;
		return;
	}

	// 在中断里面切换宽度
	ushort p = Pulses[PulseIndex++];

	// 设置宽度
	*reg = p;

	// 重复
	if(Repeated && PulseIndex >= PulseCount) PulseIndex = 0;
}

/*
#ifdef STM32F0
typedef uint32_t (*GetCap)(TIM_TypeDef* TIMx);
const static GetCap GetCapturex[4]={
TIM_GetCapture1,
TIM_GetCapture2,
TIM_GetCapture3,
TIM_GetCapture4
};
typedef void (*SetICxPres)(TIM_TypeDef* TIMx, uint16_t TIM_ICPSC);
const static SetICxPres SetICPrescaler[]={
TIM_SetIC1Prescaler ,
TIM_SetIC2Prescaler ,
TIM_SetIC3Prescaler ,
TIM_SetIC4Prescaler ,
};

Capture::Capture(Timer * timer)
{
	if(timer == NULL)return ;
	tr = timer;
//	HaveCap = 0x00;
	for(int i =0;i<4;i++)
	{
		_Handler [i]=NULL;	// 其实可以不赋值
		_Param [i]=NULL;
	}
}



void Capture::Start(int channel)
{

}


void Capture::Stop(int channel)
{

}


uint Capture :: GetCapture (int channel)
{
	if(channel >4 || channel <1)return 0;
	return (GetCapturex[channel-1])(tr->_Timer );
}
#endif

void Capture::Register(int channel,EventHandler handler, void* param )
{
	if(channel<1||channel>4) return ;
	_Handler[channel-1] = handler;
	_Param[channel-1]=param;
//	if(handler != NULL)
//		tr->Register (OnHandler  ,this);
	int irq;
	if(handler)
	{
		if(tr ->_index == 0)
			irq = TIM1_CC_IRQn;
//		else// stm32f103有个TIM8  这里留空
		Interrupt.SetPriority(irq, 1);
		Interrupt.Activate(irq, OnHandler, this);
	}
	else
	{
		_Handler[channel-1] = NULL ;
		_Param [channel-1]=NULL;
		for(int i =0 ;i<4;i++)
			if(_Handler [i] != NULL )return ;
		Interrupt.Deactivate(irq);
	}
}

// 直接用指针访问私有成员  不好
//void Capture :: OnHandler(void* sender, void* param)
//{
//	Capture * cap= (Capture*)param;
//	if(cap->_Handler != NULL)
//		cap->_Handler(sender,cap->_Param );
//}

void Capture :: OnHandler(ushort num, void* param)
{
	Capture * cap= (Capture*)param;
	if(cap != NULL)
		cap->OnInterrupt();
}

void Capture::OnInterrupt()
{
	// 找出中断源
	ushort ccx = TIM_FLAG_CC1;
	for(int i =0;i<4;i++)
	{
		if(TIM_GetFlagStatus(tr->_Timer , ccx<<i ))
			_Handler[i](this,_Param [i]);
	}
//	if(_Handler) _Handler[](this, _Param);
}

Capture::~Capture()
{
	for(int i =0;i<4;i++)
		Register(i,NULL, NULL );
	delete(tr);
}
*/
