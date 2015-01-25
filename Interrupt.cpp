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
#else
    // Enable the SYSCFG peripheral clock
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    // Remap SRAM at 0x00000000
    SYSCFG_MemoryRemapConfig(SYSCFG_MemoryRemap_SRAM);
#endif
#else*/
#ifdef STM32F4
    /*SCB->AIRCR = (0x5FA << SCB_AIRCR_VECTKEY_Pos) // 解锁
               | (7 << SCB_AIRCR_PRIGROUP_Pos);   // 没有优先组位*/
	// 中断优先级分配方案4，四位都是抢占优先级。其实上面的寄存器操作就是设置优先级方案为0
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	// 打开系统异常，否则这些异常都会跑到硬件中断里面
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA_Msk
                | SCB_SHCSR_BUSFAULTENA_Msk
                | SCB_SHCSR_MEMFAULTENA_Msk;
#endif
#ifdef STM32F1
    /*SCB->AIRCR = (0x5FA << SCB_AIRCR_VECTKEY_Pos) // 解锁
               | (7 << SCB_AIRCR_PRIGROUP_Pos);   // 没有优先组位*/
	// 中断优先级分配方案4，四位都是抢占优先级。其实上面的寄存器操作就是设置优先级方案为0
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	// 打开系统异常，否则这些异常都会跑到硬件中断里面
    SCB->SHCSR |= SCB_SHCSR_USGFAULTENA
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

// 是否在中断里面
bool TInterrupt::IsHandler() { return GetIPSR() & 0x01; }

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

extern "C"
{
	void FAULT_SubHandler(uint* registers, uint exception)
	{
		//uint exception = GetIPSR();
#ifdef STM32F0
		debug_printf("LR=0x%08x PC=0x%08x PSR=0x%08x\r\n", registers[5], registers[6], registers[7]);
		for(int i=0; i<=3; i++)
		{
			debug_printf("R%d=0x%08x\r\n", i, registers[i]);
		}
		debug_printf("R12=0x%08x\r\n", registers[4]);
#else
		debug_printf("LR=0x%08x PC=0x%08x PSR=0x%08x SP=0x%08x\r\n", registers[13], registers[14], registers[15], registers[16]);
		for(int i=0; i<=12; i++)
		{
			debug_printf("R%d=0x%08x\r\n", i, registers[i]);
		}
#endif

		if(!Sys.OnError || Sys.OnError(exception))
		{
			if(Sys.OnStop) Sys.OnStop();
			while(true);
		}
	}

	__asm void FaultHandler()
	{
		IMPORT FAULT_SubHandler

		//on entry, we have an exception frame on the stack:
		//SP+00: R0
		//SP+04: R1
		//SP+08: R2
		//SP+12: R3
		//SP+16: R12
		//SP+20: LR
		//SP+24: PC
		//SP+28: PSR
		//R0-R12 are not overwritten yet
#ifdef STM32F0
#else
		add      sp,sp,#16             // remove R0-R3
		push     {r0-r11}              // store R0-R11
#endif
		mov      r0,sp
		//R0+00: R0-R12
		//R0+52: LR
		//R0+56: PC
		//R0+60: PSR
		mrs      r1,IPSR               // exception number
		b        FAULT_SubHandler
		//never expect to return
	}
}

// 智能IRQ，初始化时备份，销毁时还原
SmartIRQ::SmartIRQ(bool enable)
{
	_state = __get_PRIMASK();
	if(enable)
		__enable_irq();
	else
		__disable_irq();
}

SmartIRQ::~SmartIRQ()
{
	__set_PRIMASK(_state);
}

// 智能锁。初始化时锁定一个整数，销毁时解锁
Lock::Lock(int& ref)
{
	Success = false;
	if(ref > 0) return;

	// 加全局锁以后再修改引用
	SmartIRQ irq;
	// 再次判断，DoubleLock双锁结构，避免小概率冲突
	if(ref > 0) return;

	_ref = &ref;
	ref++;
	Success = true;
}

Lock::~Lock()
{
	if(Success)
	{
		SmartIRQ irq;
		(*_ref)--;
	}
}

bool Lock::Wait(int us)
{
	// 可能已经进入成功
	if(Success) return true;

	int& ref = *_ref;
	// 等待超时时间
	TimeWheel tw(0, 0, us);
	while(ref > 0)
	{
		// 延迟一下，释放CPU使用权
		Sys.Sleep(1);
		if(tw.Expired()) return false;
	}

	// 加全局锁以后再修改引用
	SmartIRQ irq;
	// 再次判断，DoubleLock双锁结构，避免小概率冲突
	if(ref > 0) return false;

	ref++;
	Success = true;

	return true;
}
