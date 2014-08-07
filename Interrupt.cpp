#include "Interrupt.h"

/*
完全接管中断，在RAM中开辟中断向量表，做到随时可换。
由于中断向量表要求128对齐，这里多分配128字节，找到对齐点后给向量表使用

为了增强中断函数处理，我们使用_Vectors作为真正的中断向量表，全部使用OnHandler作为中断处理函数。
然后在OnHandler内部获取中断号，再调用Vectors中保存的用户委托，并向它传递中断号和参数。
*/
TInterrupt Interrupt;

void IntcHandler();         // 标准中断处理
uint GetInterruptNumber();  // 获取中断号
void FAULT_SubHandler();

// 真正的向量表 64k=0x10000
#ifdef STM32F0XX
	__IO Func _Vectors[VectorySize] __attribute__((at(0x20000000)));
#else
	// 84个中断向量，向上取整到2整数倍也就是128，128*4=512=0x200。CM3权威手册
	__IO Func _Vectors[VectorySize] __attribute__((__aligned__(0x200)));
#endif

#define IS_IRQ(irq) (irq >= -16 && irq <= VectorySize - 16)

void TInterrupt::Init()
{
    // 禁用所有中断
    NVIC->ICER[0] = 0xFFFFFFFF;
#ifdef STM32F10X
    NVIC->ICER[1] = 0xFFFFFFFF;
    NVIC->ICER[2] = 0xFFFFFFFF;
#endif

    // 清除所有中断位
    NVIC->ICPR[0] = 0xFFFFFFFF;
#ifdef STM32F10X
    NVIC->ICPR[1] = 0xFFFFFFFF;
    NVIC->ICPR[2] = 0xFFFFFFFF;
#endif

    _Vectors[2]  = (Func)&FAULT_SubHandler; // NMI
    _Vectors[3]  = (Func)&FAULT_SubHandler; // Hard Fault
    _Vectors[4]  = (Func)&FAULT_SubHandler; // MMU Fault
    _Vectors[5]  = (Func)&FAULT_SubHandler; // Bus Fault
    _Vectors[6]  = (Func)&FAULT_SubHandler; // Usage Fault
    _Vectors[11] = (Func)&FAULT_SubHandler; // SVC
    _Vectors[12] = (Func)&FAULT_SubHandler; // Debug
    _Vectors[14] = (Func)&FAULT_SubHandler; // PendSV
    _Vectors[15] = (Func)&FAULT_SubHandler; // Systick

#ifdef STM32F10X
    __DMB(); // 确保中断表已经被写入

    SCB->AIRCR = (0x5FA << SCB_AIRCR_VECTKEY_Pos) // 解锁
               | (7 << SCB_AIRCR_PRIGROUP_Pos);   // 没有优先组位
    SCB->VTOR = (uint)_Vectors; // 向量表基地址
    //NVIC_SetVectorTable(NVIC_VectTab_RAM, (uint)Vectors);
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA  // 打开异常
                | SCB_SHCSR_BUSFAULTENA
                | SCB_SHCSR_MEMFAULTENA;
#else
    /* Enable the SYSCFG peripheral clock*/
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    /* Remap SRAM at 0x00000000 */
    SYSCFG_MemoryRemapConfig(SYSCFG_MemoryRemap_SRAM);
#endif
}

TInterrupt::~TInterrupt()
{
	// 恢复中断向量表
#ifdef STM32F10X
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0);
#else
    SYSCFG_MemoryRemapConfig(SYSCFG_MemoryRemap_Flash);
#endif
}

bool TInterrupt::Activate(short irq, InterruptCallback isr, void* param)
{
	assert_param(IS_IRQ(irq));

    short irq2 = irq + 16; // exception = irq + 16
    _Vectors[irq2] = OnHandler;
    Vectors[irq2] = isr;
    Params[irq2] = param;

    __DMB(); // asure table is written
    //NVIC->ICPR[irq >> 5] = 1 << (irq & 0x1F); // clear pending bit
    //NVIC->ISER[irq >> 5] = 1 << (irq & 0x1F); // set enable bit
    if(irq >= 0)
    {
        NVIC_ClearPendingIRQ((IRQn_Type)irq);
        NVIC_EnableIRQ((IRQn_Type)irq);
    }

    return true;
}

bool TInterrupt::Deactivate(short irq)
{
	assert_param(IS_IRQ(irq));

    short irq2 = irq + 16; // exception = irq + 16
    Vectors[irq2] = 0;
    Params[irq2] = 0;

    //NVIC->ICER[irq >> 5] = 1 << (irq & 0x1F); // clear enable bit */
    if(irq >= 0) NVIC_DisableIRQ((IRQn_Type)irq);
    return true;
}

bool TInterrupt::Enable(short irq)
{
	assert_param(IS_IRQ(irq));

    if(irq < 0) return false;

    uint ier = NVIC->ISER[irq >> 5]; // old state
    //NVIC->ISER[irq >> 5] = 1 << (irq & 0x1F); // set enable bit
    NVIC_EnableIRQ((IRQn_Type)irq);
    return (ier >> (irq & 0x1F)) & 1; // old enable bit
}

bool TInterrupt::Disable(short irq)
{
	assert_param(IS_IRQ(irq));

    if(irq < 0) return false;

    uint ier = NVIC->ISER[irq >> 5]; // old state
    //NVIC->ICER[irq >> 5] = 1 << (irq & 0x1F); // clear enable bit
    NVIC_DisableIRQ((IRQn_Type)irq);
    return (ier >> (irq & 0x1F)) & 1; // old enable bit
}

bool TInterrupt::EnableState(short irq)
{
	assert_param(IS_IRQ(irq));

    if(irq < 0) return false;

    // return enabled bit
    return (NVIC->ISER[(uint)irq >> 5] >> ((uint)irq & 0x1F)) & 1;
}

bool TInterrupt::PendingState(short irq)
{
	assert_param(IS_IRQ(irq));

    if(irq < 0) return false;

    // return pending bit
    return (NVIC->ISPR[(uint)irq >> 5] >> ((uint)irq & 0x1F)) & 1;
}

void TInterrupt::SetPriority(short irq, uint priority)
{
	assert_param(IS_IRQ(irq));

    NVIC_SetPriority((IRQn_Type)irq, priority);
}

void TInterrupt::GetPriority(short irq)
{
	assert_param(IS_IRQ(irq));

    NVIC_GetPriority((IRQn_Type)irq);
}

void TInterrupt::GlobalEnable() { __enable_irq(); }
void TInterrupt::GlobalDisable() { __disable_irq(); }
bool TInterrupt::GlobalState() { return __get_PRIMASK(); }

#ifdef STM32F10X
uint TInterrupt::EncodePriority (uint priorityGroup, uint preemptPriority, uint subPriority)
{
    return NVIC_EncodePriority(priorityGroup, preemptPriority, subPriority);
}

void TInterrupt::DecodePriority (uint priority, uint priorityGroup, uint* pPreemptPriority, uint* pSubPriority)
{
    NVIC_DecodePriority(priority, priorityGroup, pPreemptPriority, pSubPriority);
}
#endif

void TInterrupt::OnHandler()
{
    uint num = GetInterruptNumber();
    if(num >= VectorySize) return;
    if(!Interrupt.Vectors[num]) return;

    // 找到应用层中断委托并调用
    InterruptCallback isr = (InterruptCallback)Interrupt.Vectors[num];
    void* param = (void*)Interrupt.Params[num];
    isr(num - 16, param);
}

void FAULT_SubHandler()
{
    uint exception = GetInterruptNumber();
    if (exception) {
        debug_printf("EXCEPTION 0x%02x:\r\n", exception);
    } else {
        debug_printf("ERROR:\r\n");
    }

#if DEBUG
	// 显示异常详细信息
    int i;
    if(exception==3)
        debug_printf("硬件错误\r\n");
	if(exception==5)
	{
		i = *(byte*)(0xE000ED29);
#ifdef STM32F10X
		debug_printf("\r\nBus Fault %d 0x%08x: ", i, SCB->BFAR);
#endif
		switch(i)
		{
		case 0:
			debug_printf("IBUSERR 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了"); // 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了
			break;
		case 1:
			debug_printf("PRECISERR 在数据访问期间的总线错误。通过BFAR可以获取具体的地址。发生fault的原因同上。"); // 在数据访问期间的总线错误。通过BFAR可以获取具体的地址。发生fault的原因同上。
			break;
		case 2:
			debug_printf("IMPRECISERR 与设备之间传送数据的过程中发生总线错误。可能是因为设备未经初始化而引起；或者在用户级访问了特权级的设备，或者传送的数据单位尺寸不能为设备所接受。此时，有可能是LDM/STM指令造成了非精确总线fault。"); // 与设备之间传送数据的过程中发生总线错误。可能是因为设备未经初始化而引起；或者在用户级访问了特权级的设备，或者传送的数据单位尺寸不能为设备所接受。此时，有可能是LDM/STM指令造成了非精确总线fault。
			break;
		case 3:
			debug_printf("UNSTKERR （自动）出栈期间出错。如果没有发生过STKERR，则最可能的就是在异常处理期间把SP的值破坏了"); // （自动）出栈期间出错。如果没有发生过STKERR，则最可能的就是在异常处理期间把SP的值破坏了
			break;
		case 4:
			debug_printf("STKERR （自动）入栈期间出错 1. 堆栈指针的值被破坏 2. 堆栈用量太大，到达了未定义存储器的区域 3. PSP未经初始化就使用"); // （自动）入栈期间出错 1. 堆栈指针的值被破坏 2. 堆栈用量太大，到达了未定义存储器的区域 3. PSP未经初始化就使用
			break;
		case 7:
			debug_printf("BFARVALID 表示BFAR有效"); // 表示BFAR有效
			break;
		}
		debug_printf("\r\n");
	}else if(exception==4)
	{
		i = *(byte*)(0xE000ED28);
#ifdef STM32F10X
		debug_printf("\r\nMemManage Fault %d 0x%08x: ", i, SCB->MMFAR);
#endif
		switch(i)
		{
		case 0:
			debug_printf("IACCVIOL 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了"); // 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了
			break;
		case 1:
			debug_printf("DACCVIOL 数据内存访问保护违例。这是MPU发挥作用的体现。常常是用户应用程序企图访问特权级region所致"); // 内存访问保护违例。这是MPU发挥作用的体现。常常是用户应用程序企图访问特权级region所致
			break;
		case 3:
			debug_printf("MUNSTKERR 出栈时发生错误 1. 异常服务例程破坏了堆栈指针 2. 异常服务例程更改了MPU配置"); // 出栈时发生错误 1. 异常服务例程破坏了堆栈指针 2. 异常服务例程更改了MPU配置
			break;
		case 4:
			debug_printf("MSTKERR 入栈时发生错误 1. 堆栈指针的值被破坏 2. 堆栈容易过大，已经超出MPU允许的region范围"); // 入栈时发生错误 1. 堆栈指针的值被破坏 2. 堆栈容易过大，已经超出MPU允许的region范围
			break;
		case 7:
			debug_printf("MMARVALID 表示MMAR有效"); // 表示MMAR有效
			break;
		}
		debug_printf("\r\n");
	}else if(exception==6)
	{
		i = *(byte*)(0xE000ED2A);
		debug_printf("\r\nUsage Fault %d: ", i);
		switch(i)
		{
		case 0:
			debug_printf("UNDEFINSTR 执行的指令其编码是未定义的——解码不能"); // 执行的指令其编码是未定义的——解码不能
			break;
		case 1:
			debug_printf("INVSTATE 试图切入ARM状态"); // 试图切入ARM状态
			break;
		case 2:
			debug_printf("INVPC 在异常返回时试图非法地加载EXC_RETURN到PC"); // 在异常返回时试图非法地加载EXC_RETURN到PC。包括非法的指令，非法的上下文以及非法的EXC_RETURN值。The return PC指向的指令试图设置PC的值
			break;
		case 3:
			debug_printf("NOCP 企图执行一个协处理器指令。引发此fault的指令可以从入栈的PC读取"); // 企图执行一个协处理器指令。引发此fault的指令可以从入栈的PC读取
			break;
		case 8:
			debug_printf("UNALIGNED 当UNALIGN_TRP置位时发生未对齐访问。引发此fault的指令可以从入栈的PC读取"); // 当UNALIGN_TRP置位时发生未对齐访问。引发此fault的指令可以从入栈的PC读取
			break;
		case 9:
			debug_printf("DIVBYZERO 表示除法运算时除数为零。引发此fault的指令可以从入栈的PC读取"); // 表示除法运算时除数为零（只有在DIV_0_TRP置位时才会发生）。引发此fault的指令可以从入栈的PC读取
			break;
		}
		debug_printf("\r\n");
	}
#endif

    while(true);
}

__asm uint GetInterruptNumber()
{
    mrs     r0,IPSR               // exception number
    bx      lr
}
