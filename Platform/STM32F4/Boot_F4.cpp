#include "stm32.h"

extern "C"
{
	uint32_t HSE_VALUE = 25000000;
	uint32_t SystemCoreClock = 168000000;
	__I uint8_t AHBPrescTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

	void SystemInit(void)
	{
		HSE_VALUE	= 25000000;
		SystemCoreClock	= 168000000;

		/* Configure the System clock frequency, AHB/APBx prescalers and Flash settings */
		SetSysClock(SystemCoreClock, HSE_VALUE);

		SCB->VTOR = FLASH_BASE; // Vector Table Relocation in Internal FLASH
	}

	void SetSysClock(unsigned int clock, unsigned int cystalClock)
	{
		__IO uint32_t StartUpCounter = 0, HSEStatus = 0;
		uint32_t PLL_M, PLL_N, PLL_P, PLL_Q;

		/* FPU settings ------------------------------------------------------------*/
		#if (__FPU_PRESENT == 1) && (__FPU_USED == 1)
		SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));  /* set CP10 and CP11 Full Access */
		#endif
		/* Reset the RCC clock configuration to the default reset state ------------*/
		/* Set HSION bit */
		RCC->CR |= (uint32_t)0x00000001;

		/* Reset CFGR register */
		RCC->CFGR = 0x00000000;

		/* Reset HSEON, CSSON and PLLON bits */
		RCC->CR &= (uint32_t)0xFEF6FFFF;

		/* Reset PLLCFGR register */
		RCC->PLLCFGR = 0x24003010;

		/* Reset HSEBYP bit */
		RCC->CR &= (uint32_t)0xFFFBFFFF;

		/* Disable all interrupts */
		RCC->CIR = 0x00000000;

		#ifdef DATA_IN_ExtSRAM
		SystemInit_ExtMemCtl(); 
		#endif /* DATA_IN_ExtSRAM */

		/******************************************************************************/
		/*            PLL (clocked by HSE) used as System clock source                */
		/******************************************************************************/
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
			/* Select regulator voltage output Scale 1 mode, System frequency up to 168 MHz */
			RCC->APB1ENR |= RCC_APB1ENR_PWREN;
			PWR->CR |= PWR_CR_VOS;

			/* HCLK = SYSCLK / 1*/
			RCC->CFGR |= RCC_CFGR_HPRE_DIV1;
			  
			/* PCLK2 = HCLK / 2*/
			RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;

			/* PCLK1 = HCLK / 4*/
			RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;

			// 设置 PLL
			/*
			PLL_VCO = (HSE_VALUE or HSI_VALUE / PLL_M) * PLL_N
			SYSCLK = PLL_VCO / PLL_P
			USB OTG FS, SDIO and RNG Clock =  PLL_VCO / PLLQ
			*/
			PLL_N = clock / 1000000;
			PLL_M = cystalClock / 1000000;	// 为了让它除到1
			PLL_P = 2;	// 这是分母，可用2/4/6，在系统主频不变的情况下，可以加倍扩大PLL_VC0以获取48M
			PLL_N *= PLL_P;
			// 168M分不出48M，向上加倍吧，恰巧336M可以
			while(PLL_N % 48 != 0)
			{
				PLL_P += 2;
				assert_param(PLL_P >=2 && PLL_P <= 6);
				PLL_N *= PLL_P;
			}
			PLL_Q = PLL_N / 48;	// USB等需要48M
			RCC->PLLCFGR = PLL_M | (PLL_N << 6) | (((PLL_P >> 1) -1) << 16) |
						   (RCC_PLLCFGR_PLLSRC_HSE) | (PLL_Q << 24);

			/* Enable the main PLL */
			RCC->CR |= RCC_CR_PLLON;

			/* Wait till the main PLL is ready */
			while((RCC->CR & RCC_CR_PLLRDY) == 0) { }

			/* Configure Flash prefetch, Instruction cache, Data cache and wait state */
			FLASH->ACR = FLASH_ACR_PRFTEN |FLASH_ACR_ICEN |FLASH_ACR_DCEN |FLASH_ACR_LATENCY_5WS;

			/* Select the main PLL as system clock source */
			RCC->CFGR &= (uint32_t)((uint32_t)~(RCC_CFGR_SW));
			RCC->CFGR |= RCC_CFGR_SW_PLL;

			/* Wait till the main PLL is used as system clock source */
			while ((RCC->CFGR & (uint32_t)RCC_CFGR_SWS ) != RCC_CFGR_SWS_PLL); { }
		}
	}
}
