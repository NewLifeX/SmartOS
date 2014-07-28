#include "Interrupt.h"

TInterrupt Interrupt;

uint GetException(); // 标准异常处理
void FAULT_SubHandler();

void TInterrupt::Init()
{
    // 禁用所有中断
    NVIC->ICER[0] = 0xFFFFFFFF;
    NVIC->ICER[1] = 0xFFFFFFFF;
    NVIC->ICER[2] = 0xFFFFFFFF;
    // 清除所有中断位
    NVIC->ICPR[0] = 0xFFFFFFFF;
    NVIC->ICPR[1] = 0xFFFFFFFF;
    NVIC->ICPR[2] = 0xFFFFFFFF;

    // 清零中断向量表
    memset((void*)_mem, 0, sizeof(_mem));
    //memset((void*)Vectors, 0, sizeof(Vectors));
    
    // 中断向量表要求128对齐，这里多分配128字节，找到对齐点后给向量表使用
    byte* p = (byte*)_mem;
    while((uint)p % 128 != 0) p++;
    Vectors = (uint*)p;

    Vectors[2]  = (uint)&FAULT_SubHandler; // NMI
    Vectors[3]  = (uint)&FAULT_SubHandler; // Hard Fault
    Vectors[4]  = (uint)&FAULT_SubHandler; // MMU Fault
    Vectors[5]  = (uint)&FAULT_SubHandler; // Bus Fault
    Vectors[6]  = (uint)&FAULT_SubHandler; // Usage Fault
    Vectors[11] = (uint)&FAULT_SubHandler; // SVC
    Vectors[12] = (uint)&FAULT_SubHandler; // Debug
    Vectors[14] = (uint)&FAULT_SubHandler; // PendSV
    Vectors[15] = (uint)&FAULT_SubHandler; // Systick

    //for(int i=0; i<ArrayLength(Vectors); i++)
    //    Vectors[i] = (uint)&FAULT_SubHandler;

    __DMB(); // 确保中断表已经被写入

    SCB->AIRCR = (0x5FA << SCB_AIRCR_VECTKEY_Pos) // 解锁
               | (7 << SCB_AIRCR_PRIGROUP_Pos);   // 没有优先组位
    SCB->VTOR = (uint)Vectors; // 向量表基地址
    //NVIC_SetVectorTable(NVIC_VectTab_RAM, (uint)Vectors);
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA  // 打开异常
                | SCB_SHCSR_BUSFAULTENA
                | SCB_SHCSR_MEMFAULTENA;
}

bool TInterrupt::Activate(int irq, Func isr)
{
    Vectors[irq + 16] = (uint)isr; // exception = irq + 16
    __DMB(); // asure table is written
    //NVIC->ICPR[irq >> 5] = 1 << (irq & 0x1F); // clear pending bit
    //NVIC->ISER[irq >> 5] = 1 << (irq & 0x1F); // set enable bit
    NVIC_ClearPendingIRQ((IRQn_Type)irq);
    NVIC_EnableIRQ((IRQn_Type)irq);

    return true;
}

bool TInterrupt::Deactivate(int irq)
{
    //NVIC->ICER[irq >> 5] = 1 << (irq & 0x1F); // clear enable bit */
	NVIC_DisableIRQ((IRQn_Type)irq);
    return true;
}

bool TInterrupt::Enable(int irq)
{
    uint ier = NVIC->ISER[irq >> 5]; // old state
    //NVIC->ISER[irq >> 5] = 1 << (irq & 0x1F); // set enable bit
	NVIC_EnableIRQ((IRQn_Type)irq);
    return (ier >> (irq & 0x1F)) & 1; // old enable bit
}

bool TInterrupt::Disable(int irq)
{
    uint ier = NVIC->ISER[irq >> 5]; // old state
    //NVIC->ICER[irq >> 5] = 1 << (irq & 0x1F); // clear enable bit
	NVIC_DisableIRQ((IRQn_Type)irq);
    return (ier >> (irq & 0x1F)) & 1; // old enable bit
}

bool TInterrupt::EnableState(int irq)
{
    // return enabled bit
    return (NVIC->ISER[(uint)irq >> 5] >> ((uint)irq & 0x1F)) & 1;
}

bool TInterrupt::PendingState(int irq)
{
    // return pending bit
    return (NVIC->ISPR[(uint)irq >> 5] >> ((uint)irq & 0x1F)) & 1;
}

void TInterrupt::SetPriority(int irq, uint priority)
{
    NVIC_SetPriority((IRQn_Type)irq, priority);
}

void TInterrupt::GetPriority(int irq)
{
    NVIC_GetPriority((IRQn_Type)irq);
}

uint TInterrupt::EncodePriority (uint priorityGroup, uint preemptPriority, uint subPriority)
{
    return NVIC_EncodePriority(priorityGroup, preemptPriority, subPriority);
}

void TInterrupt::DecodePriority (uint priority, uint priorityGroup, uint* pPreemptPriority, uint* pSubPriority)
{
    NVIC_DecodePriority(priority, priorityGroup, pPreemptPriority, pSubPriority);
}

void FAULT_SubHandler()
{
    uint exception = GetException();
    if (exception) {
        printf("EXCEPTION 0x%02x:\r\n", exception);
    } else {
        printf("ERROR:\r\n");
    }

#if DEBUG
	// 显示异常详细信息
    int i;
	if(exception==5)
	{
		i = *(byte*)(0xE000ED29);
		printf("\r\nBus Fault %d 0x%08x: ", i, SCB->BFAR);
		switch(i)
		{
		case 0:
			printf("IBUSERR 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了"); // 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了
			break;
		case 1:
			printf("PRECISERR 在数据访问期间的总线错误。通过BFAR可以获取具体的地址。发生fault的原因同上。"); // 在数据访问期间的总线错误。通过BFAR可以获取具体的地址。发生fault的原因同上。
			break;
		case 2:
			printf("IMPRECISERR 与设备之间传送数据的过程中发生总线错误。可能是因为设备未经初始化而引起；或者在用户级访问了特权级的设备，或者传送的数据单位尺寸不能为设备所接受。此时，有可能是LDM/STM指令造成了非精确总线fault。"); // 与设备之间传送数据的过程中发生总线错误。可能是因为设备未经初始化而引起；或者在用户级访问了特权级的设备，或者传送的数据单位尺寸不能为设备所接受。此时，有可能是LDM/STM指令造成了非精确总线fault。
			break;
		case 3:
			printf("UNSTKERR （自动）出栈期间出错。如果没有发生过STKERR，则最可能的就是在异常处理期间把SP的值破坏了"); // （自动）出栈期间出错。如果没有发生过STKERR，则最可能的就是在异常处理期间把SP的值破坏了
			break;
		case 4:
			printf("STKERR （自动）入栈期间出错 1. 堆栈指针的值被破坏 2. 堆栈用量太大，到达了未定义存储器的区域 3. PSP未经初始化就使用"); // （自动）入栈期间出错 1. 堆栈指针的值被破坏 2. 堆栈用量太大，到达了未定义存储器的区域 3. PSP未经初始化就使用
			break;
		case 7:
			printf("BFARVALID 表示BFAR有效"); // 表示BFAR有效
			break;
		}
		printf("\r\n");
	}else if(exception==4)
	{
		i = *(byte*)(0xE000ED28);
		printf("\r\nMemManage Fault %d 0x%08x: ", i, SCB->MMFAR);
		switch(i)
		{
		case 0:
			printf("IACCVIOL 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了"); // 取指访问违例 1. 内存访问保护违例。常常是用户应用程序企图访问特权级region。在这种情况下，入栈的PC给出的地址，就是产生问题的代码之所在 2. 跳转到不可执行指令的 regions 3. 异常返回时，使用了无效的EXC_RETURN值 4. 向量表中有无效的向量。例如，异常在向量建立之前就发生了，或者加载的是用于传统ARM内核的可执行映像 5. 在异常处理期间，入栈的PC值被破坏了
			break;
		case 1:
			printf("DACCVIOL 数据内存访问保护违例。这是MPU发挥作用的体现。常常是用户应用程序企图访问特权级region所致"); // 内存访问保护违例。这是MPU发挥作用的体现。常常是用户应用程序企图访问特权级region所致
			break;
		case 3:
			printf("MUNSTKERR 出栈时发生错误 1. 异常服务例程破坏了堆栈指针 2. 异常服务例程更改了MPU配置"); // 出栈时发生错误 1. 异常服务例程破坏了堆栈指针 2. 异常服务例程更改了MPU配置
			break;
		case 4:
			printf("MSTKERR 入栈时发生错误 1. 堆栈指针的值被破坏 2. 堆栈容易过大，已经超出MPU允许的region范围"); // 入栈时发生错误 1. 堆栈指针的值被破坏 2. 堆栈容易过大，已经超出MPU允许的region范围
			break;
		case 7:
			printf("MMARVALID 表示MMAR有效"); // 表示MMAR有效
			break;
		}
		printf("\r\n");
	}else if(exception==6)
	{
		i = *(byte*)(0xE000ED2A);
		printf("\r\nUsage Fault %d: ", i);
		switch(i)
		{
		case 0:
			printf("UNDEFINSTR 执行的指令其编码是未定义的——解码不能"); // 执行的指令其编码是未定义的——解码不能
			break;
		case 1:
			printf("INVSTATE 试图切入ARM状态"); // 试图切入ARM状态
			break;
		case 2:
			printf("INVPC 在异常返回时试图非法地加载EXC_RETURN到PC"); // 在异常返回时试图非法地加载EXC_RETURN到PC。包括非法的指令，非法的上下文以及非法的EXC_RETURN值。The return PC指向的指令试图设置PC的值
			break;
		case 3:
			printf("NOCP 企图执行一个协处理器指令。引发此fault的指令可以从入栈的PC读取"); // 企图执行一个协处理器指令。引发此fault的指令可以从入栈的PC读取
			break;
		case 8:
			printf("UNALIGNED 当UNALIGN_TRP置位时发生未对齐访问。引发此fault的指令可以从入栈的PC读取"); // 当UNALIGN_TRP置位时发生未对齐访问。引发此fault的指令可以从入栈的PC读取
			break;
		case 9:
			printf("DIVBYZERO 表示除法运算时除数为零。引发此fault的指令可以从入栈的PC读取"); // 表示除法运算时除数为零（只有在DIV_0_TRP置位时才会发生）。引发此fault的指令可以从入栈的PC读取
			break;
		}
		printf("\r\n");
	}
#endif
    
    while(true);
}

__asm uint GetException()
{
    mrs     r0,IPSR               // exception number
    bx      lr
}
