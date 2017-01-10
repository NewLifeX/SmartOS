#include "Sys.h"

#include "Device\Pwm.h"

#include "Platform\stm32.h"

// STM32F030 的   先这么写着 后面再对 103 做调整
typedef void (*TIM_OCInit)(TIM_TypeDef* TIMx, TIM_OCInitTypeDef* TIM_OCInitStruct);
const static TIM_OCInit OCInits[4]={TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init};

typedef void(*TIM_OCPldCfg)(TIM_TypeDef* TIMx, uint16_t TIM_OCPreload);
const static TIM_OCPldCfg TIM_OCPldCfgs[4] = { TIM_OC1PreloadConfig,TIM_OC2PreloadConfig,TIM_OC3PreloadConfig,TIM_OC4PreloadConfig };

typedef void(*SetCompare)(TIM_TypeDef* TIMx, uint32_t TIM_ICPSC);
const static SetCompare SetCompares[4] = { TIM_SetCompare1,TIM_SetCompare2,TIM_SetCompare3,TIM_SetCompare4 };

void Pwm::Config()
{
	TS("Pwm::Config");

	Timer::Config();	// 主要是配置时钟基础部分 TIM_TimeBaseInit

	auto ti	= (TIM_TypeDef*)_Timer;
	if(Opened)
		Flush();
	else
	{
		TIM_OCInitTypeDef oc;

		TIM_OCStructInit(&oc);
		oc.TIM_OCMode		= TIM_OCMode_PWM1;
		oc.TIM_OutputState	= TIM_OutputState_Enable;
		oc.TIM_OCPolarity	= Polarity ? TIM_OCPolarity_High : TIM_OCPolarity_Low;
		oc.TIM_OCIdleState	= IdleState ? TIM_OCIdleState_Reset : TIM_OCIdleState_Set;

		auto ti	= (TIM_TypeDef*)_Timer;
		for(int i=0; i<4; i++)
		{
			if(Enabled[i])
			{
				oc.TIM_Pulse	= Pulse[i];
				OCInits[i](ti, &oc);

				TIM_OCPldCfgs[i](ti, TIM_OCPreload_Enable);

				debug_printf("Pwm%d::Config %d \r\n", _index+1, i);
			}
		}
	}

	// PWM模式用不上中断  直接就丢了吧  给中断管理减减麻烦
	TIM_ITConfig(ti, TIM_IT_Update, DISABLE);
	TIM_ClearFlag(ti, TIM_FLAG_Update);

	TIM_SetCounter(ti, 0x00000000);		// 清零定时器CNT计数寄存器
	TIM_ARRPreloadConfig(ti, ENABLE);	// 使能预装载寄存器ARR
}

void Pwm::Flush()
{
	for (int i = 0; i < 4; i++)
	{
		if (Ports[i]) SetCompares[i]((TIM_TypeDef*)_Timer, Pulse[i]);
	}
}

void Pwm::Open()
{
	// 设置映射
	//if(Remap！=0) GPIO_PinRemapConfig(Remap, ENABLE);

	Timer::Open();

	if(_index == 0 ||_index == 14 ||_index == 15|| _index == 16)
		TIM_CtrlPWMOutputs((TIM_TypeDef*)_Timer, ENABLE);

	// 打开引脚
	const Pin g_Pins[][4]	=  TIM_PINS;
	//const Pin g_Pins2[][4]	=  TIM_PINS_FULLREMAP;

	// 仅支持标准引脚和完全映射，非完全映射需要外部自己初始化
	auto pss	= g_Pins;
	//if(Remap) pss	= g_Pins2;
	auto ps	= pss[_index];

	for(int i=0; i<4; i++)
	{
		if(Enabled[i] && Ports[i] == nullptr)
			Ports[i]	= new AlternatePort(ps[i]);
	}
}

void Pwm::Close()
{
	if(_index == 0 ||_index == 14 ||_index == 15|| _index == 16)
		TIM_CtrlPWMOutputs((TIM_TypeDef*)_Timer, DISABLE);

	for (int i = 0; i < 4; i++)
	{
		if (Ports[i]) TIM_OCPldCfgs[i]((TIM_TypeDef*)_Timer, TIM_OCPreload_Disable);
	}

	Timer::Close();
}

void PwmData::Config()
{
	Pwm::Config();

	// 如果需要连续调整宽度，那么需要中断
	if(Pulses) SetHandler(true);
}

void PwmData::OnInterrupt()
{
	if(!Pulses || !PulseCount || PulseIndex > PulseCount) return;

	auto ti	= (TIM_TypeDef*)_Timer;
	// 动态计算4个寄存器中的某一个，并设置宽度
	auto reg	= &(ti->CCR1);

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
