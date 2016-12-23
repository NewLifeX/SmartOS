#include "Sys.h"

#include "Interrupt.h"

#include <stdlib.h>

// 仅用于调试使用的一些函数实现，RTM不需要

//#define MEM_DEBUG DEBUG
#define MEM_DEBUG 0
#if MEM_DEBUG
	#define mem_printf debug_printf
#else
	#define mem_printf(format, ...)
#endif

#if DEBUG

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code = "SectionForSys"
	#elif defined(__GNUC__)
		__attribute__((section("SectionForSys")))
	#endif
#endif

#if MEM_DEBUG
void* malloc_(uint size)
{
	size	+= 4;
	void* p	= malloc(size);
	byte* bs = (byte*)p;
	bs[0]	= 'S';
	bs[1]	= 'M';
	*(ushort*)&bs[2]	= size;

	return &bs[4];
}

void free_(void* p)
{
	byte* bs = (byte*)p;
	bs	-= 4;
	if(!(bs[0] == 'S' && bs[1] == 'M')) mem_printf("p=0x%p bs[0]=%c bs[1]=%c\r\n", p, bs[0], bs[1]);
	assert(bs[0] == 'S' && bs[1] == 'M', "正在释放不是本系统申请的内存！");

	free(bs);
}

#else
	#define  malloc_ malloc
	#define free_ free
#endif

void* operator new(uint size)
{
    mem_printf(" new size: %d ", size);

	// 内存大小向4字节对齐
	if(size & 0x03)
	{
		size += 4 - (size & 0x03);
		mem_printf("=> %d ", size);
	}
	void* p = nullptr;
	{
		SmartIRQ irq;
		p = malloc_(size);
	}
	if(!p)
		mem_printf("malloc failed! size=%d ", size);
	else
	{
		mem_printf("0x%p ", p);
		// 如果堆只剩下64字节，则报告失败，要求用户扩大堆空间以免不测
		//uint end = (uint)&__heap_limit;
		//uint end = __get_PSP();
		//if(!end) end = __get_MSP();
		//uint end = __get_MSP();
		//if((uint)p + size + 0x40 >= end)
		//	mem_printf(" + %d near HeapEnd=0x%08x", size, end);
	}
    return p;
}

void* operator new[](uint size)
{
    mem_printf(" new size[]: %d ", size);
	// 内存大小向4字节对齐
	if(size & 0x03)
	{
		size += 4 - (size & 0x03);
		mem_printf("=> %d ", size);
	}
	void* p = nullptr;
	{
		SmartIRQ irq;
		p = malloc_(size);
	}
	if(!p)
		mem_printf("malloc failed! size=%d ", size);
	else
	{
		mem_printf("0x%p ", p);
		// 如果堆只剩下64字节，则报告失败，要求用户扩大堆空间以免不测
		//uint end = (uint)&__heap_limit;
		//uint end = __get_PSP();
		//if(!end) end = __get_MSP();
		//uint end = __get_MSP();
		//if((uint)p + size + 0x40 >= end) mem_printf(" + %d near HeapEnd=0x%08x", size, end);
	}
    return p;
}

void operator delete(void* p) noexcept
{
	mem_printf(" delete 0x%p ", p);
    if(p)
	{
		SmartIRQ irq;
		free_(p);
	}
}

void operator delete[](void* p) noexcept
{
	mem_printf(" delete[] 0x%p ", p);
    if(p)
	{
		SmartIRQ irq;
		free_(p);
	}
}

void operator delete(void* p, uint size) noexcept	{ operator delete(p); }
void operator delete[](void* p, uint size) noexcept	{ operator delete[](p); }

#if !defined(TINY) && defined(STM32F0)
	#if defined(__CC_ARM)
		#pragma arm section code
	#elif defined(__GNUC__)
		__attribute__((section("")))
	#endif
#endif

void assert_failed2(cstring msg, cstring file, unsigned int line)
{
    debug_printf("%s Line %d, %s\r\n", msg, line, file);

	TInterrupt::Halt();
}

#else

#if defined ( __CC_ARM   )
	#  include <rw/_defs.h>

// 发行版不允许抛出异常以及显示异常信息，这将极大减小使用C++标准库所带来的固件膨胀
_RWSTD_NAMESPACE_BEGIN (__rw)

void _RWSTD_EXPORT __rw_throw (int, ...)
{

}

_RWSTD_NAMESPACE_END   // __rw
#endif

#endif
