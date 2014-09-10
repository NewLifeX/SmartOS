#include "Interrupt.h"

// GD32全系列无法把向量表映射到RAM，F103只能映射到Flash别的地方
/*#if defined(GD32)// || defined(STM32F4)
	#define VEC_TABLE_ON_RAM 0
#else
	#define VEC_TABLE_ON_RAM 1
#endif*/

/*
完全接管中断，在RAM中开辟中断向量表，做到随时可换。
由于中断向量表要求128对齐，这里多分配128字节，找到对齐点后给向量表使用

为了增强中断函数处理，我们使用_Vectors作为真正的中断向量表，全部使用OnHandler作为中断处理函数。
然后在OnHandler内部获取中断号，再调用Vectors中保存的用户委托，并向它传递中断号和参数。
*/
TInterrupt Interrupt;

// 真正的向量表 64k=0x10000
/*#if VEC_TABLE_ON_RAM
#ifdef STM32F0
	__IO Func _Vectors[VectorySize] __attribute__((at(0x20000000)));
#else
	// 84个中断向量，向上取整到2整数倍也就是128，128*4=512=0x200。CM3权威手册
	__IO Func _Vectors[VectorySize] __attribute__((__aligned__(0x200)));
#endif
#endif*/

#define IS_IRQ(irq) (irq >= -16 && irq <= VectorySize - 16)

void TInterrupt::Init()
{
    // 禁用所有中断
    NVIC->ICER[0] = 0xFFFFFFFF;
#if defined(STM32F1) || defined(STM32F4)
    NVIC->ICER[1] = 0xFFFFFFFF;
    NVIC->ICER[2] = 0xFFFFFFFF;
#endif

    // 清除所有中断位
    NVIC->ICPR[0] = 0xFFFFFFFF;
#if defined(STM32F1) || defined(STM32F4)
    NVIC->ICPR[1] = 0xFFFFFFFF;
    NVIC->ICPR[2] = 0xFFFFFFFF;
#endif

/*#if VEC_TABLE_ON_RAM
	memset((void*)_Vectors, 0, VectorySize << 2);
    _Vectors[2]  = (Func)&FaultHandler; // NMI
    _Vectors[3]  = (Func)&FaultHandler; // Hard Fault
    _Vectors[4]  = (Func)&FaultHandler; // MMU Fault
    _Vectors[5]  = (Func)&FaultHandler; // Bus Fault
    _Vectors[6]  = (Func)&FaultHandler; // Usage Fault
    _Vectors[11] = (Func)&FaultHandler; // SVC
    _Vectors[12] = (Func)&FaultHandler; // Debug
    _Vectors[14] = (Func)&FaultHandler; // PendSV
    _Vectors[15] = (Func)&FaultHandler; // Systick

#if defined(STM32F1) || defined(STM32F4)
    __DMB(); // 确保中断表已经被写入

    SCB->AIRCR = (0x5FA << SCB_AIRCR_VECTKEY_Pos) // 解锁
               | (7 << SCB_AIRCR_PRIGROUP_Pos);   // 没有优先组位
    //SCB->VTOR = (uint)_Vectors; // 向量表基地址
    NVIC_SetVectorTable(NVIC_VectTab_RAM, (uint)((byte*)_Vectors - SRAM_BASE));
	assert_param(SCB->VTOR == (uint)_Vectors);
#ifdef STM32F4
	// 不能使用以下代码，否则F4里面无法响应中断
    //SCB->SHCSR |= SCB_SHCSR_USGFAULTACT_Msk  // 打开异常
    //            | SCB_SHCSR_BUSFAULTACT_Msk
    //            | SCB_SHCSR_MEMFAULTACT_Msk;
#else
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA  // 打开异常
                | SCB_SHCSR_BUSFAULTENA
                | SCB_SHCSR_MEMFAULTENA;
#endif
#else
    // Enable the SYSCFG peripheral clock
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    // Remap SRAM at 0x00000000
    SYSCFG_MemoryRemapConfig(SYSCFG_MemoryRemap_SRAM);
#endif
#else*/
#ifdef STM32F1
    SCB->AIRCR = (0x5FA << SCB_AIRCR_VECTKEY_Pos) // 解锁
               | (7 << SCB_AIRCR_PRIGROUP_Pos);   // 没有优先组位
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA  // 打开异常
                | SCB_SHCSR_BUSFAULTENA
                | SCB_SHCSR_MEMFAULTENA;
#endif
//#endif

	// 初始化EXTI中断线为默认值
	EXTI_DeInit();
}

TInterrupt::~TInterrupt()
{
	// 恢复中断向量表
#if defined(STM32F1) || defined(STM32F4)
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0);
#else
    SYSCFG_MemoryRemapConfig(SYSCFG_MemoryRemap_Flash);
#endif
}

bool TInterrupt::Activate(short irq, InterruptCallback isr, void* param)
{
	assert_param(IS_IRQ(irq));

    short irq2 = irq + 16; // exception = irq + 16
/*#if VEC_TABLE_ON_RAM
    _Vectors[irq2] = UserHandler;
#endif*/
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

__asm uint GetIPSR()
{
    mrs     r0,IPSR               // exception number
    bx      lr
}

void UserHandler()
{
    uint num = GetIPSR();
    //if(num >= VectorySize) return;
    //if(!Interrupt.Vectors[num]) return;
	assert_param(num < VectorySize);
	assert_param(Interrupt.Vectors[num]);

	// 内存检查
#if DEBUG
	Sys.CheckMemory();
#endif

    // 找到应用层中断委托并调用
    InterruptCallback isr = (InterruptCallback)Interrupt.Vectors[num];
    void* param = (void*)Interrupt.Params[num];
    isr(num - 16, param);
}

void FaultHandler()
{
    uint exception = GetIPSR();
    /*if (exception) {
        debug_printf("EXCEPTION 0x%02x:\r\n", exception);
    } else {
        debug_printf("ERROR:\r\n");
    }

#if DEBUG
	ShowFault(exception);
#endif*/

	if(!Sys.OnError || Sys.OnError(exception))
	{
		if(Sys.OnStop) Sys.OnStop();
		while(true);
	}
}
