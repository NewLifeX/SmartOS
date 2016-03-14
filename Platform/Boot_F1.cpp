#include "stm32.h"

extern "C"
{
	unsigned int HSE_VALUE = 8000000;
	uint32_t SystemCoreClock = 72000000;
	__I uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

	__weak void SystemInit(void)
	{
		HSE_VALUE = 8000000;
		SystemCoreClock = 72000000;

		/* Configure the System clock frequency, AHB/APBx prescalers and Flash settings */
		SetSysClock(SystemCoreClock, HSE_VALUE);

		SCB->VTOR = FLASH_BASE; // Vector Table Relocation in Internal FLASH
	}

	void SetSysClock(unsigned int clock, unsigned int cystalClock)
	{
		__IO uint32_t StartUpCounter = 0, HSEStatus = 0;
		uint32_t mull, mull2;

		/* Reset the RCC clock configuration to the default reset state(for debug purpose) */
		/* Set HSION bit */
		RCC->CR |= (uint32_t)0x00000001;

		/* Reset SW, HPRE, PPRE1, PPRE2, ADCPRE and MCO bits */
#ifndef STM32F10X_CL
		RCC->CFGR &= (uint32_t)0xF8FF0000;
#else
		RCC->CFGR &= (uint32_t)0xF0FF0000;
#endif /* STM32F10X_CL */   

		/* Reset HSEON, CSSON and PLLON bits */
		RCC->CR &= (uint32_t)0xFEF6FFFF;

		/* Reset HSEBYP bit */
		RCC->CR &= (uint32_t)0xFFFBFFFF;

		/* Reset PLLSRC, PLLXTPRE, PLLMUL and USBPRE/OTGFSPRE bits */
		RCC->CFGR &= (uint32_t)0xFF80FFFF;

#ifdef STM32F10X_CL
		/* Reset PLL2ON and PLL3ON bits */
		RCC->CR &= (uint32_t)0xEBFFFFFF;

		/* Disable all interrupts and clear pending bits  */
		RCC->CIR = 0x00FF0000;

		/* Reset CFGR2 register */
		RCC->CFGR2 = 0x00000000;
#elif defined (STM32F10X_LD_VL) || defined (STM32F10X_MD_VL) || (defined STM32F10X_HD_VL)
		/* Disable all interrupts and clear pending bits  */
		RCC->CIR = 0x009F0000;

		/* Reset CFGR2 register */
		RCC->CFGR2 = 0x00000000;      
#else
		/* Disable all interrupts and clear pending bits  */
		RCC->CIR = 0x009F0000;
#endif /* STM32F10X_CL */

#if defined (STM32F10X_HD) || (defined STM32F10X_XL) || (defined STM32F10X_HD_VL)
	#ifdef DATA_IN_ExtSRAM
		SystemInit_ExtMemCtl(); 
	#endif /* DATA_IN_ExtSRAM */
#endif 

		/* SYSCLK, HCLK, PCLK2 and PCLK1 configuration ---------------------------*/    
		/* Enable HSE */    
		RCC->CR |= ((uint32_t)RCC_CR_HSEON);

		/* Wait till HSE is ready and if Time out is reached exit */
		do
		{
			HSEStatus = RCC->CR & RCC_CR_HSERDY;
			StartUpCounter++;  
		} while((HSEStatus == 0) && (StartUpCounter != HSE_STARTUP_TIMEOUT));

		if ((RCC->CR & RCC_CR_HSERDY) != RESET)
			HSEStatus = (uint32_t)0x01;
		else
			HSEStatus = (uint32_t)0x00;

		if (HSEStatus == (uint32_t)0x01)
		{
			/* Enable Prefetch Buffer */
			FLASH->ACR |= FLASH_ACR_PRFTBE;

			/* Flash 2 wait state */
			FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
			FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_2;    


			/* HCLK = SYSCLK */
			RCC->CFGR |= (uint32_t)RCC_CFGR_HPRE_DIV1;

			/* PCLK2 = HCLK */
			RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE2_DIV1;

			/* PCLK1 = HCLK */
			RCC->CFGR |= (uint32_t)RCC_CFGR_PPRE1_DIV2;

#ifdef STM32F10X_CL
			/* Configure PLLs ------------------------------------------------------*/
			/* PLL2 configuration: PLL2CLK = (HSE / 5) * 8 = 40 MHz */
			/* PREDIV1 configuration: PREDIV1CLK = PLL2 / 5 = 8 MHz */

			RCC->CFGR2 &= (uint32_t)~(RCC_CFGR2_PREDIV2 | RCC_CFGR2_PLL2MUL |
								  RCC_CFGR2_PREDIV1 | RCC_CFGR2_PREDIV1SRC);
			RCC->CFGR2 |= (uint32_t)(RCC_CFGR2_PREDIV2_DIV5 | RCC_CFGR2_PLL2MUL8 |
								 RCC_CFGR2_PREDIV1SRC_PLL2 | RCC_CFGR2_PREDIV1_DIV5);

			/* Enable PLL2 */
			RCC->CR |= RCC_CR_PLL2ON;
			/* Wait till PLL2 is ready */
			while((RCC->CR & RCC_CR_PLL2RDY) == 0) { }

			/* PLL configuration: PLLCLK = PREDIV1 * 9 = 72 MHz */ 
			RCC->CFGR &= (uint32_t)~(RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLSRC | RCC_CFGR_PLLMULL);
			RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLXTPRE_PREDIV1 | RCC_CFGR_PLLSRC_PREDIV1 | 
								RCC_CFGR_PLLMULL9); 
#else    
			/*  PLL configuration: PLLCLK = HSE * 9 = 72 MHz */
			RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLXTPRE |
											RCC_CFGR_PLLMULL));
			//RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | RCC_CFGR_PLLMULL9);
			// 支持超频，主频必须是8M的倍频
			mull = clock / cystalClock;
			mull2 = (mull - 2) << 18;
			// 处理0.5倍频
			if( (mull * cystalClock + cystalClock / 2) == clock )
			{
				mull = 2 * clock / cystalClock;
				mull2 = (mull - 2 ) << 18 | RCC_CFGR_PLLXTPRE_HSE_Div2;
			}

			RCC->CFGR |= (uint32_t)(RCC_CFGR_PLLSRC_HSE | mull2);
#endif /* STM32F10X_CL */

			/* Enable PLL */
			RCC->CR |= RCC_CR_PLLON;

			/* Wait till PLL is ready */
			while((RCC->CR & RCC_CR_PLLRDY) == 0) { }

			/* Select PLL as system clock source */
			RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
			RCC->CFGR |= (uint32_t)RCC_CFGR_SW_PLL;    

			/* Wait till PLL is used as system clock source */
			while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS) != (uint32_t)0x08) { }
		}
	}
}
