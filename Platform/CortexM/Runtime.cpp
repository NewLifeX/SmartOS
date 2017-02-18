#include "Kernel\Sys.h"
#include "Kernel\Interrupt.h"
#include "Kernel\Heap.h"

#include <stdlib.h>

//#define MEM_DEBUG DEBUG
#define MEM_DEBUG 1
#if MEM_DEBUG
	#define mem_printf debug_printf
#else
	#define mem_printf(format, ...)
#endif

#pragma arm section rwdata = ".InRoot"
// 全局堆
Heap* _Heap	= nullptr;

// 关键性代码，放到开头

extern "C" {
	INROOT void* malloc(uint size)
	{
		// 初始化全局堆
		if(!_Heap)
		{
			uint heap	= Sys.HeapBase();
			uint stack	= Sys.StackTop();
			static Heap g_Heap(heap, stack - heap);
			_Heap	= &g_Heap;

			mem_printf("Heap::Init(0x%08X, %d)\r\n", heap, stack - heap);
		}

	#if MEM_DEBUG
		size	+= 4;
	#endif
		void* p	= _Heap->Alloc(size);
	#if MEM_DEBUG
		byte* bs = (byte*)p;
		bs[0]	= 'S';
		bs[1]	= 'M';
		*(ushort*)&bs[2]	= size;
		p	= &bs[4];
	#endif

		return p;
	}

	INROOT void free(void* p)
	{
	#if MEM_DEBUG
		byte* bs = (byte*)p;
		bs	-= 4;
		if(!(bs[0] == 'S' && bs[1] == 'M')) mem_printf("p=0x%p bs[0]=%c bs[1]=%c\r\n", p, bs[0], bs[1]);
		assert(bs[0] == 'S' && bs[1] == 'M', "正在释放不是本系统申请的内存！");
		p	= bs;
	#endif

		_Heap->Free(p);
	}
}

INROOT void* operator new(uint size)
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
		p = malloc(size);
	}
	if(!p)
		mem_printf("malloc failed! size=%d ", size);
	else
		mem_printf("0x%p ", p);

    return p;
}

INROOT void* operator new[](uint size)
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
		p = malloc(size);
	}
	if(!p)
		mem_printf("malloc failed! size=%d ", size);
	else
		mem_printf("0x%p ", p);

	return p;
}

INROOT void operator delete(void* p) noexcept
{
	mem_printf(" delete 0x%p ", p);
    if(p)
	{
		SmartIRQ irq;
		free(p);
	}
}

INROOT void operator delete[](void* p) noexcept
{
	mem_printf(" delete[] 0x%p ", p);
    if(p)
	{
		SmartIRQ irq;
		free(p);
	}
}

INROOT void operator delete(void* p, uint size) noexcept	{ operator delete(p); }
INROOT void operator delete[](void* p, uint size) noexcept	{ operator delete[](p); }

void assert_failed2(cstring msg, cstring file, unsigned int line)
{
    debug_printf("%s Line %d, %s\r\n", msg, line, file);

	TInterrupt::Halt();
}

extern "C" {
	// 这里的静态对象不需要析构
	int __aeabi_atexit(int a1, int a2, int a3)
	{
		return 0;
	}
}
