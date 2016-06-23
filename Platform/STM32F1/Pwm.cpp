#include "Sys.h"

#include "Pwm.h"

#include "Platform\stm32.h"

/*================ Pwm ================*/

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

void Pwm::Config()
{
	TS("Pwm::Config");

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

void Pwm::FlushOut()
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

void Pwm::Open()
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

void Pwm::Close()
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

void Pwm::OnInterrupt()
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
	if(timer == nullptr)return ;
	tr = timer;
//	HaveCap = 0x00;
	for(int i =0;i<4;i++)
	{
		_Handler [i]=nullptr;	// 其实可以不赋值
		_Param [i]=nullptr;
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
//	if(handler != nullptr)
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
		_Handler[channel-1] = nullptr ;
		_Param [channel-1]=nullptr;
		for(int i =0 ;i<4;i++)
			if(_Handler [i] != nullptr )return ;
		Interrupt.Deactivate(irq);
	}
}

// 直接用指针访问私有成员  不好
//void Capture :: OnHandler(void* sender, void* param)
//{
//	Capture * cap= (Capture*)param;
//	if(cap->_Handler != nullptr)
//		cap->_Handler(sender,cap->_Param );
//}

void Capture :: OnHandler(ushort num, void* param)
{
	Capture * cap= (Capture*)param;
	if(cap != nullptr)
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
		Register(i,nullptr, nullptr );
	delete(tr);
}
*/
