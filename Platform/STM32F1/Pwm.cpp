#include "Sys.h"

#include "Device\Pwm.h"

#include "Platform\stm32.h"

/*================ Pwm ================*/

// STM32F030 的   先这么写着 后面再对 103 做调整
typedef void (*TIM_OCInit)(TIM_TypeDef* TIMx, TIM_OCInitTypeDef* TIM_OCInitStruct);
const static TIM_OCInit OCInits[4]={TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init};

typedef void(*TIM_OCPldCfg)(TIM_TypeDef* TIMx, uint16_t TIM_OCPreload);
const static TIM_OCPldCfg TIM_OCPldCfgs[4] = { TIM_OC1PreloadConfig,TIM_OC2PreloadConfig,TIM_OC3PreloadConfig,TIM_OC4PreloadConfig, };

typedef void(*SetCompare)(TIM_TypeDef* TIMx, uint16_t TIM_ICPSC);
const static SetCompare SetCompares[4] = { TIM_SetCompare1,TIM_SetCompare2,TIM_SetCompare3,TIM_SetCompare4, };

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
			if (Configed & 0x1 << i)
			{
				SetCompares[i]((TIM_TypeDef*)_Timer, Pulse[i]);
			}
			else
			{
				oc.TIM_Pulse = Pulse[i];
				OCInits[i](ti, &oc);

				TIM_OCPldCfgs[i](ti, TIM_OCPreload_Enable);

				Configed |= 0x1 << i;
				debug_printf("InitPWM[%d]\r\n",i);
			}
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
	for (int i = 0; i < 4; i++)
	{
		if (Pulse[i] != 0xFFFF)
		{
			SetCompares[i]((TIM_TypeDef*)_Timer,Pulse[i]);
		}
	}
}

void Pwm::Open()
{
	Timer::Open();

	if(_index == 0 ||_index == 14 ||_index == 15|| _index == 16)
		TIM_CtrlPWMOutputs((TIM_TypeDef*)_Timer, ENABLE);
}

void Pwm::Close()
{
	if(_index == 0 ||_index == 14 ||_index == 15|| _index == 16)
		TIM_CtrlPWMOutputs((TIM_TypeDef*)_Timer, DISABLE);


	for (int i = 0; i < 4; i++)
	{
		if (Configed |= 0x1 << i)
		{
			TIM_OCPldCfgs[i]((TIM_TypeDef*)_Timer, TIM_OCPreload_Disable);
			Configed &= ~(0x1 << i);
		}
	}

	Timer::Close();
}

void Pwm::OnInterrupt()
{
	if(!Pulses || !PulseCount || PulseIndex > PulseCount) return;

	auto ti	= (TIM_TypeDef*)_Timer;
	// 动态计算4个寄存器中的某一个，并设置宽度
	volatile ushort* reg = &(ti->CCR1);

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
//STM32F103C8T6  Time3PWM输出初始化 可用
//arr：自动重装值
//psc：时钟预分频数
void Time3PWM_Init(u16 arr,u16 psc)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA  | RCC_APB2Periph_GPIOB  | RCC_APB2Periph_AFIO, ENABLE);  //使能GPIO外设和AFIO复用功能模块时钟使能
	//用于TIM3的CH2输出的PWM通过该LED显示
	//设置该引脚为复用输出功能
	//GPIO_PinRemapConfig(GPIO_FullRemap_TIM3,ENABLE);
	GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3,ENABLE);	// PB0 PB1 PB4 PB5 都是复用引脚

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;  	//复用推挽输出
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0; 			//TIM3_CH3
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1; 			//TIM3_CH4
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable,ENABLE);	// PB4 是 NJTRST
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4; 				//TIM3_CH1
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5; 			//TIM3_CH2
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	TIM_TimeBaseStructure.TIM_Period = arr; 						//设置在下一个更新事件装入活动的自动重装载寄存器周期的值	 80K
	TIM_TimeBaseStructure.TIM_Prescaler =psc; 						//设置用来作为TIMx时钟频率除数的预分频值  不分频
	TIM_TimeBaseStructure.TIM_ClockDivision = 0; 					//设置时钟分割:TDTS = Tck_tim
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;  	//TIM向上计数模式
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure); 				//根据TIM_TimeBaseInitStruct中指定的参数初始化TIMx的时间基数单位

	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM2; 				//选择定时器模式:TIM脉冲宽度调制模式2  TIM_OCMode_PWM2  TIM_OCMode_PWM1  波形为反向
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; 	//比较输出使能
	TIM_OCInitStructure.TIM_Pulse = 0; 								//设置待装入捕获比较寄存器的脉冲值
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High; 		//输出极性:TIM输出比较极性高

	TIM_OC1Init(TIM3, &TIM_OCInitStructure);  						//根据TIM_OCInitStruct中指定的参数初始化外设TIMx
	TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);  				//使能TIMx在CCR2上的预装载寄存器

	TIM_OC2Init(TIM3, &TIM_OCInitStructure);  						//根据TIM_OCInitStruct中指定的参数初始化外设TIMx
	TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);  				//使能TIMx在CCR2上的预装载寄存器

	TIM_OC3Init(TIM3, &TIM_OCInitStructure);  						//根据TIM_OCInitStruct中指定的参数初始化外设TIMx
	TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);  				//使能TIMx在CCR2上的预装载寄存器

	TIM_OC4Init(TIM3, &TIM_OCInitStructure);  						//根据TIM_OCInitStruct中指定的参数初始化外设TIMx
	TIM_OC4PreloadConfig(TIM3, TIM_OCPreload_Enable);  				//使能TIMx在CCR2上的预装载寄存器

	TIM_ARRPreloadConfig(TIM3, ENABLE); //使能TIMx在ARR上的预装载寄存器
	TIM_Cmd(TIM3, ENABLE);  //使能TIMx外设
}

更新PWM输出函数 可用
TIM_SetCompare1(TIM3,led0pwmval);
TIM_SetCompare2(TIM3,led0pwmval);
TIM_SetCompare3(TIM3,led0pwmval);
TIM_SetCompare4(TIM3,led0pwmval);

*/
