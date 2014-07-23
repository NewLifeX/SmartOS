#include "System.h"

#ifndef BIT
    #define BIT(x)	(1 << (x))
#endif

#define  RCC_CFGR_USBPRE_Div1                     ((uint32_t)0x00400000)        /*!< USB Device prescaler */
#define  RCC_CFGR_USBPRE_1Div5                    ((uint32_t)0x00000000)        /*!< USB Device prescaler */
#define  RCC_CFGR_USBPRE_Div2                     ((uint32_t)0x00C00000)        /*!< USB Device prescaler */
#define  RCC_CFGR_USBPRE_2Div5                    ((uint32_t)0x00800000)        /*!< USB Device prescaler */

#define GD32_PLL_MASK	0x20000000

byte fac_us;//全局变量		
volatile uint TickStat;//全局变量	 为延时函数服务

/****************************************************
函数功能：延时初始化
输入参数：SYSCLK : 系统时钟
输出参数：无
备    注：无
*****************************************************/
void delay_init(uint clk)
{
	 NVIC_InitTypeDef NVIC_InitStructure;
	 /****************************************
	 *SystemFrequency/1000      1ms中断一次  *
	 *SystemFrequency/100000    10us中断一次 *
	 *SystemFrequency/1000000   1us中断一次  *
	 *****************************************/
   SysTick->CTRL &=~BIT(2);//选择外部时钟
	 SysTick->CTRL |=BIT(1);//开启定时器减到0后的中断请求
	 fac_us = clk/8;//计算好SysTick加载值
	
/*初始化部分代码*/
    SysTick->LOAD = (uint)fac_us*1000-1;//加载时间值
    SysTick->VAL = 1;//随便写个值，清除加载寄存器的值
    SysTick->CTRL |= BIT(0);//SysTick使能

	   /* Configure the NVIC Preemption Priority Bits */  
    NVIC_InitStructure.NVIC_IRQChannel = (byte)SysTick_IRQn;
#ifdef STM32F10X
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x00;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;
#else
    NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00;			//想在中断里使用延时函数就必须让此中断优先级最高
#endif
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

__asm void ForceDisabled()
{
/*
    register UINT32 Primask __asm("primask");
    UINT32 m = Primask;
    __disable_irq();
    return m ^ 1;
*/
#ifdef STM32F10X							//stm32f030的汇编与此有所区别
    MRS      r0,PRIMASK
    CPSID    i
    EOR      r0,r0,#1
    BX       lr
#else
    BX       lr
#endif
}
__asm void ForceEnabled()
{
/*
    register UINT32 Primask __asm("primask");
    UINT32 m = Primask;
    __enable_irq();
    return m ^ 1;
*/
	// 这里打开中断以后，经常发生各种各样的异常，没有关系，并不是这里的错，而是之前可能就已经发生了异常，只不过无法产生中断，等到这里而已
#ifdef STM32F10X							//stm32f030的汇编与此有所区别
    MRS      r0,PRIMASK	
    CPSIE    i
    EOR      r0,r0,#1
    BX       lr
#else
    BX       lr
	
#endif
}


//利用SysTick定时器产生1ms时基

// SysTick_Handler  		滴答定时器中断
void SysTick_Handler(void)				//需要最高优先级  必须有抢断任何其他中断的能力才能  //供其他中断内延时使用
{
	if(TickStat)
		TickStat--;
}
/****************************************************
函数功能：ms级延时
输入参数：nms : 毫秒
输出参数：无
备    注：调用此函数前，需要初始化Delay_Init()函数
*****************************************************/							    
void delay_ms(uint nms)
{
	TickStat=nms;
	while(TickStat);
}
/****************************************************
函数功能：us级延时
输入参数：nus : 微秒
输出参数：无
备    注：调用此函数前，需要初始化Delay_Init()函数
*****************************************************/		    								   
void delay_us(uint nus)
{		
	uint t=SysTick->CTRL&0x00ffffff;		//SysTick  为24位倒计数定时器
	uint tc=fac_us*nus;
	//if(nus<100){for();retrun}					//低于100微秒的延时就没有必要使用定时器作为时基了
	if(nus>1000)										//如果延迟超过1ms 
	{
		delay_ms(nus/1000);
//		if(tc%1000==0)return;				//几率比较小  忽略此句、
		tc=(tc%1000)*fac_us;				 
	}
	if((t-tc)>0)
	{
		while(SysTick->CTRL>(t-tc));
		return;
	}
	else if((t-tc)==0)
	{
		while(SysTick->CTRL);
		return;
	}
	else 
	{
		tc= SysTick->LOAD -(tc-t);				//低于1s  所以
		while(SysTick->CTRL!=tc);
		return;
	}
}
#if GD32F1
// 获取JTAG编号，ST是0x041，GD是0x7A3
uint16_t Get_JTAG_ID()
{
    if( *( uint8_t *)( 0xE00FFFE8 ) & 0x08 )
    {
        return  ((*(uint8_t *)(0xE00FFFD0) & 0x0F ) << 8 ) |
                ((*(uint8_t *)(0xE00FFFE4) & 0xFF ) >> 3 ) |
                ((*(uint8_t *)(0xE00FFFE8) & 0x07 ) << 5 ) + 1 ;
    }
    return  0;
}

void STM32_BootstrapCode()
{
    uint n = 0;
    uint RCC_CFGR_ADC_BITS = RCC_CFGR_ADCPRE_DIV8;
    uint FLASH_ACR_LATENCY_BITS = 0;
    uint RCC_CFGR_USBPRE_BIT = 0;
    uint mull, mull2;
    // 是否GD
    bool isGD = Get_JTAG_ID() == 0x7A3;
    if(isGD && Sys.Clock == 72000000) Sys.Clock = 120000000;
    // 确保关闭中断，为了方便调试，Debug时不关闭
#ifndef DEBUG
    //__disable_irq();
#endif
    n = Sys.Clock / 14000000;
    if (n < 6)
        RCC_CFGR_ADC_BITS = RCC_CFGR_ADCPRE_DIV6;
    else
        RCC_CFGR_ADC_BITS = RCC_CFGR_ADCPRE_DIV8;
    // GD32的Flash零等待
    if (isGD)
        FLASH_ACR_LATENCY_BITS = FLASH_ACR_LATENCY_0; // 不允许等待
    else
        FLASH_ACR_LATENCY_BITS = FLASH_ACR_LATENCY_2; // 等待两个
    
    // 配置JTAG调试支持
    DBGMCU->CR = DBGMCU_CR_DBG_TIM2_STOP | DBGMCU_CR_DBG_SLEEP;

    // 允许访问未对齐内存，不强制8字节栈对齐
    SCB->CCR &= ~(SCB_CCR_UNALIGN_TRP | SCB_CCR_STKALIGN);

    // 捕获所有异常
    CoreDebug->DEMCR |= CoreDebug_DEMCR_VC_HARDERR_Msk | CoreDebug_DEMCR_VC_INTERR_Msk |
                        CoreDebug_DEMCR_VC_BUSERR_Msk | CoreDebug_DEMCR_VC_STATERR_Msk |
                        CoreDebug_DEMCR_VC_CHKERR_Msk | CoreDebug_DEMCR_VC_NOCPERR_Msk |
                        CoreDebug_DEMCR_VC_MMERR_Msk;

    // 为了配置时钟，CPU必须运行在8MHz晶振上
    RCC->CR |= RCC_CR_HSION;
    while(!(RCC->CR & RCC_CR_HSIRDY));

	// 切换到内部时钟
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
	// RCC_CFGR_SW_HSI是0，一点意义都没有
    RCC->CFGR |= RCC_CFGR_SW_HSI;      // sysclk = AHB = APB1 = APB2 = HSI (8MHz)

    // 关闭 HSE 时钟
    RCC->CR &= ~RCC_CR_HSEON;
    // 关闭 HSE & PLL
    RCC->CR &= ~RCC_CR_PLLON;

	RCC->CR |= ((uint32_t)RCC_CR_HSEON);
	while(!(RCC->CR & RCC_CR_HSERDY));

    // 设置Flash访问时间，并打开预读缓冲
    FLASH->ACR = FLASH_ACR_LATENCY_BITS | FLASH_ACR_PRFTBE;

    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
	// 支持超频，主频必须是8M的倍频
	mull = Sys.Clock / Sys.CystalClock;
	mull2 = (mull - 2) << 18;
	// 处理0.5倍频
	if( (mull * Sys.CystalClock + Sys.CystalClock / 2) == Sys.Clock )
	{
		mull = 2 * Sys.Clock / Sys.CystalClock;
		// 对于GD32的108MHz，没有除尽。
		if( isGD && mull > 17 )
			mull2 = (mull - 17) << 18 | RCC_CFGR_PLLXTPRE_HSE_Div2 | GD32_PLL_MASK;
		else
			mull2 = (mull - 2 ) << 18 | RCC_CFGR_PLLXTPRE_HSE_Div2;
	}

	RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | mull2);

    switch ( 2 * Sys.Clock / 48000000 )
    {
        case 2:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_Div1;
			break;
        case 3:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_1Div5;
			break;
        case 4:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_Div2;
			break;
        case 5:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_2Div5;
			break;
        default:
            RCC_CFGR_USBPRE_BIT = RCC_CFGR_USBPRE_Div1;
			break;
    }

    // 最终时钟配置
    RCC->CFGR |= RCC_CFGR_USBPRE_BIT     // USB clock
              | RCC_CFGR_HPRE_DIV1  // AHB clock
              | RCC_CFGR_PPRE1_DIV2 // APB1 clock
              | RCC_CFGR_PPRE2_DIV1 // APB2 clock
              | RCC_CFGR_ADC_BITS;      // ADC clock (max 14MHz)
    
    // 打开 HSE & PLL
    RCC->CR |= RCC_CR_PLLON;// | RCC_CR_HSEON;

    // 等到 PPL 达到时钟频率
    while(!(RCC->CR & RCC_CR_PLLRDY));

    // 最终时钟配置
    RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
    RCC->CFGR |= RCC_CFGR_SW_PLL;         // sysclk = pll out (SYSTEM_CLOCK_HZ)
	while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08);

    // 最小化外设时钟
    RCC->AHBENR  = RCC_AHBENR_SRAMEN | RCC_AHBENR_FLITFEN;
    RCC->APB2ENR = RCC_APB2ENR_AFIOEN;
    RCC->APB1ENR = RCC_APB1ENR_PWREN;

    // 关闭 HSI 时钟
    RCC->CR &= ~RCC_CR_HSION;

	// 计算当前工作频率
	mull = RCC->CFGR & ((int)0x3C0000 | GD32_PLL_MASK);
	if((mull & GD32_PLL_MASK) != 0) // 兼容GD32的108MHz
		mull = (((mull)&(0x003C0000)) >> 18) + 17;
	else
		mull = ( mull >> 18) + 2;
	//Sys.Frequency = HSI_VALUE * pllmull;
	// 不能直接用HSI_VALUE，因为不同的硬件设备使用不同的晶振
	Sys.Clock = Sys.CystalClock * mull;
	if( (RCC->CFGR & RCC_CFGR_PLLXTPRE_HSE_Div2) && !isGD ) Sys.Clock /= 2;
}
#endif

void TCore_Init(TCore* this)
{
    Sys.Sleep = delay_ms;
    Sys.Delay = delay_us;
    Sys.EnableInterrupts = ForceEnabled;
    Sys.DisableInterrupts = ForceDisabled;

#if GD32F1
	STM32_BootstrapCode();
#endif
    delay_init(Sys.Clock/1000000);
}
