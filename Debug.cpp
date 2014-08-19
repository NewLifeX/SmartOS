#include "Sys.h"
// 仅用于调试使用的一些函数实现，RTM不需要

#if DEBUG

extern uint __heap_base;
extern uint __heap_limit;

void* operator new(uint size)
{
    debug_printf(" new size: %d ", size);
    void * p = malloc(size);
	if(!p)
		debug_printf("malloc failed! size=%d ", size);
	else
	{
		debug_printf("0x%08x ", p);
		// 如果堆只剩下64字节，则报告失败，要求用户扩大堆空间以免不测
		uint end = (uint)&__heap_limit;
		if((uint)p + size + 0x40 >= end) debug_printf(" + %d near HeapEnd=0x%08x", size, end);
	}
	assert_param(p);
    return p;
}

void* operator new[](uint size)
{
    debug_printf(" new size[]: %d ", size);
    void * p = malloc(size);
	if(!p)
		debug_printf("malloc failed! size=%d ", size);
	else
	{
		debug_printf("0x%08x ", p);
		// 如果堆只剩下64字节，则报告失败，要求用户扩大堆空间以免不测
		uint end = (uint)&__heap_limit;
		if((uint)p + size + 0x40 >= end) debug_printf(" + %d near HeapEnd=0x%08x", size, end);
	}
	assert_param(p);
    return p;
}

void operator delete(void * p)
{
	debug_printf(" delete 0x%08x ", p);
    if(p) free(p);
}

void operator delete[](void * p)
{
	debug_printf(" delete[] 0x%08x ", p);
    if(p) free(p);
}

#ifdef  USE_FULL_ASSERT

/**
  * @brief  Reports the name of the source file and the source line number
  *   where the assert_param error has occurred.
  * @param file: pointer to the source file name
  * @param line: assert_param error line source number
  * @retval : None
  */
void assert_failed(uint8_t* file, uint32_t line)
{
    /* User can add his own implementation to report the file name and line number,
    ex: debug_printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /*if(_printf_sp) */debug_printf("Assert Failed! Line %d, %s\r\n", line, file);

    /* Infinite loop */
    while (1) { }
}
#endif


#endif
