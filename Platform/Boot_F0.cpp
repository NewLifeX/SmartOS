#include "stm32.h"

extern "C"
{
	uint32_t SystemCoreClock    = 48000000;
	__I uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

	void SystemInit (void)
	{
		SystemCoreClock = 48000000;

		/* Configure the System clock frequency, AHB/APBx prescalers and Flash settings */
		SetSysClock(SystemCoreClock, 8000000);
	}

	void SetSysClock(unsigned int clock, unsigned int cystalClock)
	{
		__IO uint32_t StartUpCounter = 0, HSEStatus = 0;
		uint32_t mull, pll;

		/* Set HSION bit */
		RCC->CR |= (uint32_t)0x00000001;

		/* Reset SW[1:0], HPRE[3:0], PPRE[2:0], ADCPRE and MCOSEL[2:0] bits */
		RCC->CFGR &= (uint32_t)0xF8FFB80C;

		/* Reset HSEON, CSSON and PLLON bits */
		RCC->CR &= (uint32_t)0xFEF6FFFF;

		/* Reset HSEBYP bit */
		RCC->CR &= (uint32_t)0xFFFBFFFF;

		/* Reset PLLSRC, PLLXTPRE and PLLMUL[3:0] bits */
		RCC->CFGR &= (uint32_t)0xFFC0FFFF;

		/* Reset PREDIV1[3:0] bits */
		RCC->CFGR2 &= (uint32_t)0xFFFFFFF0;

		/* Reset USARTSW[1:0], I2CSW, CECSW and ADCSW bits */
		RCC->CFGR3 &= (uint32_t)0xFFFFFEAC;

		/* Reset HSI14 bit */
		RCC->CR2 &= (uint32_t)0xFFFFFFFE;

		/* Disable all interrupts */
		RCC->CIR = 0x00000000;

		/* SYSCLK, HCLK, PCLK configuration ----------------------------------------*/
		/* Enable HSE */
		RCC->CR |= ((uint32_t)RCC_CR_HSEON);

		/* Wait till HSE is ready and if Time out is reached exit */
		do
		{
			HSEStatus = RCC->CR & RCC_CR_HSERDY;
			StartUpCounter++;
		} while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

		if ((RCC->CR & RCC_CR_HSERDY) != RESET)
		{
			HSEStatus = (uint32_t)0x01;
		}
		else
		{
			HSEStatus = (uint32_t)0x00;
		}

		if (HSEStatus != (uint32_t)0x01) return;

		/* Enable Prefetch Buffer and set Flash Latency */
		FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY;

		/* HCLK = SYSCLK */
		RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;

		/* PCLK = HCLK */
		RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE_DIV1;

		/* PLL configuration = HSE * 6 = 48 MHz */
		RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL));
		//RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_PREDIV1 | RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLMULL6);
		// 支持多种倍频
		mull = clock / cystalClock;
		pll = ((mull - 2) * 4) << 16;
		RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_PREDIV1 | RCC_CFGR_PLLXTPRE_PREDIV1 | pll);
		//SystemCoreClock = cystalClock * mull;

		/* Enable PLL */
		RCC->CR |= RCC_CR_PLLON;

		/* Wait till PLL is ready */
		while((RCC->CR & RCC_CR_PLLRDY) == 0) { }

		/* Select PLL as system clock source */
		RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
		RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;

		/* Wait till PLL is used as system clock source */
		while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)RCC_CFGR_SWS_PLL) { }
	}

#if !VEC_TABLE_ON_RAM
	void NMI_Handler()			{ FaultHandler(); }
	void HardFault_Handler()	{ FaultHandler(); }
	void MemManage_Handler()	{ FaultHandler(); }
	void BusFault_Handler()		{ FaultHandler(); }
	void UsageFault_Handler()	{ FaultHandler(); }
	void SVC_Handler()			{ UserHandler(); }
	void PendSV_Handler()		{ UserHandler(); }
	void SysTick_Handler()		{ UserHandler(); }
	void WWDG_IRQHandler()		{ UserHandler(); }
	void PVD_IRQHandler()		{ UserHandler(); }
	void RTC_IRQHandler()		{ UserHandler(); }
	void FLASH_IRQHandler()		{ UserHandler(); }
	void RCC_IRQHandler()		{ UserHandler(); }
	void EXTI0_1_IRQHandler()	{ UserHandler(); }
	void EXTI2_3_IRQHandler()	{ UserHandler(); }
	void EXTI4_15_IRQHandler()	{ UserHandler(); }
	void TS_IRQHandler()		{ UserHandler(); }
	void DMA1_Channel1_IRQHandler()		{ UserHandler(); }
	void DMA1_Channel2_3_IRQHandler()	{ UserHandler(); }
	void DMA1_Channel4_5_IRQHandler()	{ UserHandler(); }
	void ADC1_COMP_IRQHandler()	{ UserHandler(); }
	void TIM1_BRK_UP_TRG_COM_IRQHandler()	{ UserHandler(); }
	void TIM1_CC_IRQHandler()	{ UserHandler(); }
	void TIM2_IRQHandler()		{ UserHandler(); }
	void TIM3_IRQHandler()		{ UserHandler(); }
	void TIM6_DAC_IRQHandler()	{ UserHandler(); }
	void TIM14_IRQHandler()		{ UserHandler(); }
	void TIM15_IRQHandler()		{ UserHandler(); }
	void TIM16_IRQHandler()		{ UserHandler(); }
	void TIM17_IRQHandler()		{ UserHandler(); }
	void I2C1_IRQHandler()		{ UserHandler(); }
	void I2C2_IRQHandler()		{ UserHandler(); }
	void SPI1_IRQHandler()		{ UserHandler(); }
	void SPI2_IRQHandler()		{ UserHandler(); }
	void USART1_IRQHandler()	{ UserHandler(); }
	void USART2_IRQHandler()	{ UserHandler(); }
	void CEC_IRQHandler()		{ UserHandler(); }
#endif
}
