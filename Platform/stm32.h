#ifndef __STM32_H
#define __STM32_H

#if defined (STM32F0XX_LD) || defined (STM32F030X6) || defined (STM32F0XX_MD) || defined (STM32F030X8)
    #ifndef STM32F0XX
        #define STM32F0XX
    #endif
#endif

#ifdef STM32F10X_HD
    #ifndef STM32F10X
        #define STM32F10X
    #endif
#endif

#ifdef STM32F0XX
    #ifndef STM32F0
        #define STM32F0
    #endif
#else
    #ifdef STM32F0
        #define STM32F0XX
    #endif
#endif

#ifdef STM32F10X
    #ifndef STM32F1
        #define STM32F1
    #endif
#else
    #ifdef STM32F1
        #define STM32F10X
    #endif
#endif

#ifdef STM32F4XX
    #ifndef STM32F4
        #define STM32F4
    #endif
#else
    #ifdef STM32F4
        #define STM32F4XX
    #endif
#endif

// 默认使用固件库
#ifndef USE_STDPERIPH_DRIVER
#define USE_STDPERIPH_DRIVER
#endif

#if defined(STM32F4)
	#include "stm32f4xx.h"
#elif defined(STM32F2)
	#include "stm32f2xx.h"
#elif defined(STM32F1)
	#include "stm32f10x.h"
#elif defined(STM32F3)
	#include "stm32f3xx.h"
#elif defined(STM32F0)
	#include "stm32f0xx.h"
#else
	#error "请在Keil项目配置C/C++页定义芯片平台，如STM32F0/STM32F1/STM32F4"
#endif

extern "C"
{
	void SetSysClock(unsigned int clock, unsigned int cystalClock);
}

#endif
