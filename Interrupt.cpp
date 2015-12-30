#include "Interrupt.h"
#include "SerialPort.h"

TInterrupt Interrupt;

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

	// 初始化EXTI中断线为默认值
	EXTI_DeInit();
}

/*TInterrupt::~TInterrupt()
{
	// 恢复中断向量表
#if defined(STM32F1) || defined(STM32F4)
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0);
#else
    SYSCFG_MemoryRemapConfig(SYSCFG_MemoryRemap_Flash);
#endif
}*/

bool TInterrupt::Activate(short irq, InterruptCallback isr, void* param)
{
	assert_param(IS_IRQ(irq));

    short irq2 = irq + 16; // exception = irq + 16
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

#ifdef STM32F1
uint TInterrupt::EncodePriority (uint priorityGroup, uint preemptPriority, uint subPriority)
{
    return NVIC_EncodePriority(priorityGroup, preemptPriority, subPriority);
}

void TInterrupt::DecodePriority (uint priority, uint priorityGroup, uint* pPreemptPriority, uint* pSubPriority)
{
    NVIC_DecodePriority(priority, priorityGroup, pPreemptPriority, pSubPriority);
}
#endif

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

__asm uint GetIPSR()
{
    mrs     r0,IPSR               // exception number
    bx      lr
}

// 是否在中断里面
bool TInterrupt::IsHandler() { return GetIPSR() & 0x01; }

#ifdef TINY
__asm void FaultHandler() { }
#else
void UserHandler()
{
    uint num = GetIPSR();
	assert_param(num < VectorySize);
	//assert_param(Interrupt.Vectors[num]);
	if(!Interrupt.Vectors[num]) return;

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
	/*  在GD32F130C8中时常有一个FAULT_SubHandler跳转的错误，
	经查是因为FAULT_SubHandler和FaultHandler离得太远，导致b跳转无法到达，
	因此把这两个函数指定到同一个端中，使得它们地址分布上紧挨着
	*/
	void FAULT_SubHandler(uint* registers, uint exception)
	{
#ifdef STM32F0
		debug_printf("LR=0x%08x PC=0x%08x PSR=0x%08x\r\n", registers[5], registers[6], registers[7]);
		for(int i=0; i<=7; i++)
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

#if DEBUG
		ShowFault(exception);

		TraceStack::Show();

		SerialPort* sp = SerialPort::GetMessagePort();
		if(sp) sp->Flush();
#endif
		while(true);
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
#if defined(STM32F0) || defined(GD32F150)
		//add      sp,sp,#16             // remove R0-R3
		push     {r4-r7}              // store R0-R11
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
#endif

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

#pragma arm section code

/*================================ 锁 ================================*/

#include "Time.h"

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
	tw.Sleep = 1;
	while(ref > 0)
	{
		// 延迟一下，释放CPU使用权
		//Sys.Sleep(1);
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

/*================================ 跟踪栈 ================================*/

#if DEBUG

static TArray<const char*, 0x40>* _TS = NULL;

TraceStack::TraceStack(const char* name)
{
	static TArray<const char*, 0x40> __ts(0);
	_TS	= &__ts;

	_TS->Push(name);
}

TraceStack::~TraceStack()
{
	_TS->Pop();
}

void TraceStack::Show()
{
	debug_printf("TraceStack::Show:\r\n");
	if(_TS)
	{
		for(int i=_TS->Length() - 1; i>=0; i--)
		{
			debug_printf("\t<=%s \r\n", (*_TS)[i]);
		}
	}
}

#endif
