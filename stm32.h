#ifndef __STM32_H
#define __STM32_H

#ifdef STM32F0XX_LD
    #ifndef STM32F0XX
        #define STM32F0XX
    #endif
#endif

#ifdef STM32F10X_HD
    #ifndef STM32F10X
        #define STM32F10X
    #endif
#endif

#if defined(STM32F4XX)
	#include "stm32f4xx.h"
#elif defined(STM32F2XX)
	#include "stm32f2xx.h"
#elif defined(STM32F10X)
	#include "stm32f10x.h"
#elif defined(STM32F3XX)
	#include "stm32f3xx.h"
#elif defined(STM32F0XX)
	#include "stm32f0xx.h"
#else
	#include "stm32f10x.h"
#endif

#endif
