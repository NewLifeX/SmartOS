#include "Sys.h"

#include "Platform\stm32.h"

#include <stdlib.h>

// 仅用于调试使用的一些函数实现，RTM不需要

#if DEBUG

extern uint __heap_base;
extern uint __heap_limit;
extern uint __Vectors;
extern uint __Vectors_End;
extern uint __Vectors_Size;

#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t* file, unsigned int line)
{
    debug_printf("Assert Failed! Line %d, %s\r\n", line, file);

	TInterrupt::Halt();
}

/*bool assert_ptr_(const void* p)
{
	if((uint)p < FLASH_BASE)
	{
		debug_printf("ptr:0x%08x < FLASH_BASE:0x%08x\r\n", p, FLASH_BASE);
		return false;
	}

	uint ramEnd = SRAM_BASE + (Sys.RAMSize << 10);
	if(Sys.RAMSize > 0 && (uint)p >= ramEnd)
	{
		debug_printf("ptr:0x%08x >= SRAM_END:0x%08x\r\n", p, ramEnd);
		return false;
	}

	// F4有64k的CCM内存
#if defined(STM32F4)
	if((uint)p >= 0x10000000 && (uint)p < 0x10010000) return true;
#endif

	uint flashEnd = FLASH_BASE + (Sys.FlashSize << 10);
	if(Sys.FlashSize > 0 && (uint)p >= flashEnd && (uint)p < SRAM_BASE)
	{
		debug_printf("ptr:0x%08x >= FLASH_END:0x%08x\r\n", p, flashEnd);
		return false;
	}

	// 不支持静态全局对象
	//if(p <= (void*)&__heap_base) return false;

	return true;
}*/
#endif

#endif
