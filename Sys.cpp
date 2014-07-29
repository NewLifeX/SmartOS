#include "Sys.h"

#include "Time.h"

TSys Sys;
Time* g_Time;

#ifndef BIT
    #define BIT(x)	(1 << (x))
#endif

#define  RCC_CFGR_USBPRE_Div1                     ((uint32_t)0x00400000)        /*!< USB Device prescaler */
#define  RCC_CFGR_USBPRE_1Div5                    ((uint32_t)0x00000000)        /*!< USB Device prescaler */
#define  RCC_CFGR_USBPRE_Div2                     ((uint32_t)0x00C00000)        /*!< USB Device prescaler */
#define  RCC_CFGR_USBPRE_2Div5                    ((uint32_t)0x00800000)        /*!< USB Device prescaler */

#define GD32_PLL_MASK	0x20000000

__asm bool TSys::DisableInterrupts()
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
__asm bool TSys::EnableInterrupts()
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

void TSys::Sleep(uint ms) { g_Time->Sleep(ms * 1000); }

void TSys::Delay(uint us) { g_Time->Sleep(us); }

void TSys::Reset() { NVIC_SystemReset(); }

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

#if defined(GD32) && defined(STM32F10X)
void Bootstrap()
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
	//TSys::Frequency = HSI_VALUE * pllmull;
	// 不能直接用HSI_VALUE，因为不同的硬件设备使用不同的晶振
	Sys.Clock = Sys.CystalClock * mull;
	if( (RCC->CFGR & RCC_CFGR_PLLXTPRE_HSE_Div2) && !isGD ) Sys.Clock /= 2;
}
#endif

void ShowError(int code) { printf("系统错误！%d\r\n", code); }

TSys::TSys()
{
#if DEBUG
    Debug = true;
#else
    Debug = false;
#endif

    Clock = 72000000;
    CystalClock = 8000000;    // 晶振时钟
    MessagePort = 0; // COM1;

    ID[0] = *(__IO uint *)(0X1FFFF7F0); // 高字节
    ID[1] = *(__IO uint *)(0X1FFFF7EC); // 
    ID[2] = *(__IO uint *)(0X1FFFF7E8); // 低字节
    //MCUID = *(__IO uint *)(0xE0042000); // MCU编码。低字设备版本，高字子版本
    MCUID = DBGMCU->IDCODE; // MCU编码。低字设备版本，高字子版本
    FlashSize = *(__IO ushort *)(0x1FFFF7E0);  // 容量
    IsGD = Get_JTAG_ID() == 0x7A3;

    if(IsGD) Clock = 120000000;
    
#if DEBUG
    OnError = ShowError;
#else
    OnError = 0;
#endif

    Interrupt.Init();
    g_Time = new Time();
}

TSys::~TSys()
{
    if(g_Time) delete g_Time;
    g_Time = NULL;
}

void TSys::Init(void)
{
    // 获取当前频率
    RCC_ClocksTypeDef clock;

    RCC_GetClocksFreq(&clock);
#if defined(GD32) && defined(STM32F10X)
    // 如果当前频率不等于配置，则重新配置时钟
	if(Clock != clock.SYSCLK_Frequency) Bootstrap();
#else
    Clock = clock.SYSCLK_Frequency;
#endif

#ifdef STM32F10X
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4);	//中断优先级分配方案4   四位都是抢占优先级
#endif
}

extern "C"
{
    void HardFault_Handler()
    {
        if(Sys.OnError) Sys.OnError(1);
    }

    /*void Reset_Handler()
    {
        Bootstrap();
        //SystemInit();
        //__main();
        //main();
    }*/
}
