#ifndef __STM32_H
#define __STM32_H

#if !defined(STM32F0) && !defined(STM32F1) && !defined(STM32F4) && !defined(GD32F150)
	#if defined (STM32F0XX) || defined (STM32F0XX_LD) || defined (STM32F030X6) || defined (STM32F0XX_MD) || defined (STM32F030X8)
		#ifndef STM32F0
			#define STM32F0
		#endif
	#endif

	#if defined (STM32F10X_HD) || defined (STM32F10X) || defined (STM32F10X_MD) || defined (STM32F1XX) || defined (STM32F1XX_MD)
		#ifndef STM32F1
			#define STM32F1
		#endif
	#endif

	#ifdef STM32F4XX
		#ifndef STM32F4
			#define STM32F4
		#endif
	#endif

	// GD32F1x0最接近STM32F0
	#ifdef GD32F1x0
		#ifndef STM32F0
			#define STM32F0
		#endif
		#ifndef GD32
			#define GD32
		#endif
	#endif
	
	#ifdef GD32F150X8
		#define GD32F150
		#define GD32
	#endif
#endif

#ifdef STM32F4
	#define STM32F4XX
#endif

// 默认使用固件库
#ifndef USE_STDPERIPH_DRIVER
	#define USE_STDPERIPH_DRIVER
#endif

#if defined(_MSC_VER)
	#define __ASM            __asm
	#define __INLINE         inline
	#define __WFI

	static __INLINE void __DSB() { }
	static __INLINE void __DMB() { }
#endif

#if defined(STM32F4)
	#include "stm32f4xx.h"
	#include "Pin_STM32F4.h"
#elif defined(STM32F2)
	#include "stm32f2xx.h"
#elif defined(STM32F1)
	#include "stm32f10x.h"
	#include "STM32F1/Pin_STM32F1.h"
#elif defined(STM32F3)
	#include "stm32f3xx.h"
#elif defined(STM32F0)
	#include "stm32f0xx.h"
	#include "Pin_STM32F0.h"
#elif defined(GD32F150)
	#include "stm32f0xx.h"
#else
	#error "请在Keil项目配置C/C++页定义芯片平台，如STM32F0/STM32F1/STM32F2/STM32F3/STM32F4/GD32F150"
#endif

extern "C"
{
	void SystemInit(void);
	void SetSysClock(unsigned int clock, unsigned int cystalClock);
}

#endif
