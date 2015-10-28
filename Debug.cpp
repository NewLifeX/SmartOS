#include "Sys.h"
// 仅用于调试使用的一些函数实现，RTM不需要

#define MEM_DEBUG DEBUG
#if MEM_DEBUG
	#define mem_printf debug_printf
#else
	#define mem_printf(format, ...)
#endif

#pragma arm section code = "SectionForSys"

extern uint __heap_base;
extern uint __heap_limit;
extern uint __Vectors;
extern uint __Vectors_End;
extern uint __Vectors_Size;

void* operator new(uint size)
{
    mem_printf(" new size: %d ", size);
	// 内存大小向4字节对齐
	if(size & 0x03)
	{
		size += 4 - (size & 0x03);
		mem_printf("=> %d ", size);
	}
	void* p = NULL;
	{
		SmartIRQ irq;
		p = malloc(size);
	}
	if(!p)
		mem_printf("malloc failed! size=%d ", size);
	else
	{
		mem_printf("0x%08x ", p);
		// 如果堆只剩下64字节，则报告失败，要求用户扩大堆空间以免不测
		//uint end = (uint)&__heap_limit;
		//uint end = __get_PSP();
		//if(!end) end = __get_MSP();
		uint end = __get_MSP();
		if((uint)p + size + 0x40 >= end)
			mem_printf(" + %d near HeapEnd=0x%08x", size, end);
	}
	assert_param(p);
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
	void* p = NULL;
	{
		SmartIRQ irq;
		p = malloc(size);
	}
	if(!p)
		mem_printf("malloc failed! size=%d ", size);
	else
	{
		mem_printf("0x%08x ", p);
		// 如果堆只剩下64字节，则报告失败，要求用户扩大堆空间以免不测
		//uint end = (uint)&__heap_limit;
		//uint end = __get_PSP();
		//if(!end) end = __get_MSP();
		uint end = __get_MSP();
		if((uint)p + size + 0x40 >= end) mem_printf(" + %d near HeapEnd=0x%08x", size, end);
	}
	assert_param(p);
    return p;
}

void operator delete(void* p)
{
	assert_ptr(p);

	mem_printf(" delete 0x%08x ", p);
    if(p)
	{
		SmartIRQ irq;
		free(p);
	}
}

void operator delete[](void* p)
{
	assert_ptr(p);

	mem_printf(" delete[] 0x%08x ", p);
    if(p)
	{
		SmartIRQ irq;
		free(p);
	}
}

#if DEBUG

#pragma arm section code

#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t* file, uint32_t line)
{
    debug_printf("Assert Failed! Line %d, %s\r\n", line, file);

    while (1) { }
}

void assert_failed(const char* msg, uint8_t* file, uint32_t line)
{
    debug_printf("%s Line %d, %s\r\n", msg, line, file);

    while (1) { }
}

extern uint __heap_base;

bool assert_ptr_(const void* p)
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
}
#endif

void ShowFault(uint exception)
{
	// 显示异常详细信息
    int i;
    if(exception==3)
	{
		// 0xE000ED2C
 		uint n = *(uint*)(SCB_BASE + 0x2C);
        debug_printf("\r\n硬件错误 0x%x ", n);
		if(n & (1<<1))
		{
			debug_printf("在取向量时发生\r\n");
		}
		else if(n & (1u<<30))
		{
			debug_printf("是总线fault，存储器管理fault 或是用法fault 上访的结果\r\n");
			// GD不能映射中断向量表，必须使用Flash开头的那个默认中断向量表，而这需要在Keil的ARM属性页设置GD32=1
			// __Vectors_Size只是一个标记，需要先取地址，才得到它的值
			//if(Sys.IsGD && (uint)&__Vectors_Size <= 7 * 4)
			{
				debug_printf("GD不能映射中断向量表，必须使用Flash开头的那个默认中断向量表，而这需要在Keil的ARM属性页设置GD32=1\r\n");
			}
		}
		else if(n & (1u<<31))
		{
			debug_printf("因调试事件而产生\r\n");
		}
	}
	else if(exception==5)
	{
		i = *(byte*)(SCB_BASE + 0x29);
#if defined(STM32F1) || defined(STM32F4)
		debug_printf("\r\nBus Fault %d 0x%08x: \r\n", i, SCB->BFAR);
#endif
		if(i & (1<<0))
		{
			debug_printf("IBUSERR 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。 5. 在异常处理期间，入栈的PC值被破坏了\r\n"); // 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了
		}
		else if(i & (1<<1))
		{
			debug_printf("PRECISERR 在数据访问期间的总线错误。通过BFAR可以获取具体的地址。发生fault的原因同上。\r\n"); // 在数据访问期间的总线错误。通过BFAR可以获取具体的地址。发生fault的原因同上。
		}
		else if(i & (1<<2))
		{
			debug_printf("IMPRECISERR 与设备之间传送数据的过程中发生总线错误。可能是因为设备未经初始化而引起；或者在用户级访问了特权级的设备，或者传送的数据单位尺寸不能为设备所接受。此时，有可能是LDM/STM指令造成了非精确总线fault。\r\n"); // 与设备之间传送数据的过程中发生总线错误。可能是因为设备未经初始化而引起；或者在用户级访问了特权级的设备，或者传送的数据单位尺寸不能为设备所接受。此时，有可能是LDM/STM指令造成了非精确总线fault。
		}
		else if(i & (1<<3))
		{
			debug_printf("UNSTKERR （自动）出栈期间出错。如果没有发生过STKERR，则最可能的就是在异常处理期间把SP的值破坏了\r\n"); // （自动）出栈期间出错。如果没有发生过STKERR，则最可能的就是在异常处理期间把SP的值破坏了
		}
		else if(i & (1<<4))
		{
			debug_printf("STKERR （自动）入栈期间出错 1. 堆栈指针的值被破坏 2. 堆栈用量太大，到达了未定义存储器的区域 3. PSP未经初始化就使用\r\n"); // （自动）入栈期间出错 1. 堆栈指针的值被破坏 2. 堆栈用量太大，到达了未定义存储器的区域 3. PSP未经初始化就使用
		}
		else if(i & (1<<5))
		{
			debug_printf("BFARVALID 表示BFAR有效\r\n"); // 表示BFAR有效
		}
		//debug_printf("\r\n");
	}
	else if(exception==4)
	{
		i = *(byte*)(SCB_BASE + 0x28);
#if defined(STM32F1) || defined(STM32F4)
		debug_printf("\r\nMemManage Fault %d 0x%08x: \r\n", i, SCB->MMFAR);
#endif
		if(i & (1<<0))
		{
			debug_printf("IACCVIOL 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。 5. 在异常处理期间，入栈的PC值被破坏了\r\n"); // 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了
		}
		else if(i & (1<<1))
		{
			debug_printf("DACCVIOL 数据内存访问保护违例。一般是访问非法内存位置所致。可以注意筛查野指针问题。\r\n"); // 内存访问保护违例。这是MPU发挥作用的体现。常常是用户应用程序企图访问特权级region所致
		}
		else if(i & (1<<3))
		{
			debug_printf("MUNSTKERR 出栈时发生错误 1. 异常服务例程破坏了堆栈指针 2. 异常服务例程更改了MPU配置\r\n"); // 出栈时发生错误 1. 异常服务例程破坏了堆栈指针 2. 异常服务例程更改了MPU配置
		}
		else if(i & (1<<4))
		{
			debug_printf("MSTKERR 入栈时发生错误 1. 堆栈指针的值被破坏 2. 堆栈容易过大，已经超出MPU允许的region范围\r\n"); // 入栈时发生错误 1. 堆栈指针的值被破坏 2. 堆栈容易过大，已经超出MPU允许的region范围
		}
		else if(i & (1<<7))
		{
			debug_printf("MMARVALID 表示MMAR有效\r\n"); // 表示MMAR有效
		}
	}
	else if(exception==6)
	{
		i = *(byte*)(SCB_BASE + 0x2A);
		debug_printf("\r\nUsage Fault %d: \r\n", i);
		if(i & (1<<0))
		{
			debug_printf("UNDEFINSTR 执行的指令其编码是未定义的——解码不能\r\n"); // 执行的指令其编码是未定义的——解码不能
		}
		else if(i & (1<<1))
		{
			debug_printf("INVSTATE 试图切入ARM状态\r\n"); // 试图切入ARM状态
		}
		else if(i & (1<<2))
		{
			debug_printf("INVPC 在异常返回时试图非法地加载EXC_RETURN到PC\r\n"); // 在异常返回时试图非法地加载EXC_RETURN到PC。包括非法的指令，非法的上下文以及非法的EXC_RETURN值。The return PC指向的指令试图设置PC的值
		}
		else if(i & (1<<3))
		{
			debug_printf("NOCP 企图执行一个协处理器指令。引发此fault的指令可以从入栈的PC读取\r\n"); // 企图执行一个协处理器指令。引发此fault的指令可以从入栈的PC读取
		}
		else if(i & (1<<4))
		{
			debug_printf("UNALIGNED 当UNALIGN_TRP置位时发生未对齐访问。引发此fault的指令可以从入栈的PC读取\r\n"); // 当UNALIGN_TRP置位时发生未对齐访问。引发此fault的指令可以从入栈的PC读取
		}
		else if(i & (1<<5))
		{
			debug_printf("DIVBYZERO 表示除法运算时除数为零。引发此fault的指令可以从入栈的PC读取\r\n"); // 表示除法运算时除数为零（只有在DIV_0_TRP置位时才会发生）。引发此fault的指令可以从入栈的PC读取
		}
	}
}

#endif
