#include "Kernel\Thread.h"
#include "Kernel\Task.h"
#include "Kernel\Interrupt.h"

#include "Platform\stm32.h"

//#define TH_DEBUG DEBUG
#define TH_DEBUG 0

#ifndef FPU
	#ifdef STM32F4
		#define FPU 1
	#endif
#endif

#ifdef FPU
	#define STACK_Size ((18 + 8 + 16 + 8) << 2);	// 0xC8 = 200
#else
	#define STACK_Size ((8 + 8) << 2)	// 0x40 = 64
#endif

#ifdef FPU
	#define STACK_SAVE_Size ((18 + 8) << 2)	// 0x68 = 104
#else
	#define STACK_SAVE_Size (8 << 2)	// 0x20 = 32
#endif

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

void Thread::CheckStack()
{
#ifdef DEBUG
	uint p = __get_PSP();
	// 不用考虑寄存器保护部分，前面还有恰好足够的空间留给它们
	/*#ifdef STM32F4
	p -= 0x40;
	#endif
	p -= 0x20;*/

	uint stackBottom = (uint)StackTop - StackSize;
	if(p < stackBottom)
		debug_printf("Thread::CheckStack %d %s Overflow, Stack 0x%08x < 0x%08x\r\n", ID, Name, p, stackBottom);
	assert(stackBottom <= p, "StackSize");
#endif
}

// 系统线程调度开始
void Thread::OnSchedule()
{
	// 使用双栈。每个线程有自己的栈，属于PSP，中断专用MSP
	if(__get_CONTROL() != 2)
	{
		// 设置PSP，使用双栈
		// 很多RTOS在这里设置PSP为0，然后这个函数最后是一个死循环。
		// 其实，PSP为0以后，这个函数就无法正常退出了，我们把它设置为MSP，确保函数正常退出，外部死循环
		__set_PSP(__get_MSP());
		// 这个时候的这个PSP是个祸害，首次中断会往这里压栈保存寄存器，我们申请一块不再回收的空间给它
		//__set_PSP((uint)Thread_PSP + STACK_SAVE_Size);
		__set_CONTROL(2);
		__ISB();
	}

	// 有些说法这里MSP要8字节对齐。有些霸道。
	__set_MSP(__get_MSP() & 0xFFFFFFF8);
}

extern "C"
{
	extern uint** curStack;	// 当前线程栈的指针。需要保存线程栈，所以需要指针
	extern uint* newStack;	// 新的线程栈

#ifdef STM32F0
	__asm void PendSV_Handler()
	{
		IMPORT curStack
		IMPORT newStack

		CPSID   I                   // 关闭全局中断

		LDR		R2, =curStack
		LDR		R2, [R2]

		CMP		R2, #0x00
		BEQ		PendSV_NoSave

		MRS     R0, PSP

		SUBS    R0, R0, #0x20       // 保存r4-11到线程栈   r0-3 pc等在中断时被压栈了
		STM		R0!,{R4-R7}			// 先保存R4~R7，R0地址累加
		MOV     R4, R8
		MOV     R5, R9
		MOV     R6, R10
		MOV     R7, R11
		STMIA   R0!,{R4-R7}			// 把R8~R11挪到R4~R7，再次保存
		SUBS    R0, R0, #0x20

		STR     R0, [R2]			// 备份当前sp到任务控制块

PendSV_NoSave						// 此时整个上下文已经被保存
		LDR		R3, =newStack
		LDR		R0, [R3]

		ADDS    R0, R0, #16			// 从新的栈中恢复 r4-11
		LDM     R0!, {R4-R7}		// 先把高地址的4单元恢复出来，它们就是R8~R11
		MOV     R8,  R4
		MOV     R9,  R5
		MOV     R10, R6
		MOV     R11, R7
		SUBS    R0, R0, #32			// 跑回到底地址的4单元，这个才是真正的R8~R11
		LDMIA   R0!, {R4-R7}
		ADDS    R0, R0, #16

		MSR     PSP, R0             // 修改PSP

		MOVS    R0, #4				// 确保中断返回用户栈
		MOVS    R0, #4
		RSBS    R0, #0

		CPSIE   I
		BX      LR                  // 中断返回将恢复上下文
	}
#else
	__asm void PendSV_Handler()
	{
		IMPORT curStack
		IMPORT newStack

		CPSID   I                   // 关闭全局中断

		LDR		R2, =curStack
		LDR		R2, [R2]

		CBZ     R2, PendSV_NoSave	// 如果当前线程栈为空则不需要保存。实际上不可能
		NOP

		MRS     R0, PSP
#ifdef FPU
		VSTMFD	r0!, {d8 - d15} 	// 压入 FPU 寄存器 s16~s31，寄存器增加
#endif

		SUBS    R0, R0, #0x20       // 保存r4-11到线程栈   r0-3 pc等在中断时被压栈了
		STM     R0, {R4-R11}

		STR     R0, [R2]			// 备份当前sp到任务控制块

PendSV_NoSave						// 此时整个上下文已经被保存
		LDR		R3, =newStack
		LDR		R0, [R3]
		LDM     R0, {R4-R11}        // 从新的栈中恢复 r4-11
		ADDS    R0, R0, #0x20

#ifdef FPU
		VLDMFD	r0!, {d8 - d15} 	// 弹出 FPU 寄存器 s16~s31
#endif

		MSR     PSP, R0             // 修改PSP
		ORR     LR, LR, #0x04       // 确保中断返回用户栈

		CPSIE   I
		BX      LR                  // 中断返回将恢复上下文
	}
#endif
}

// 切换线程，马上切换时间片给下一个线程
bool Thread::CheckPend()
{
	// 如果有挂起的切换，则不再切换。否则切换时需要保存的栈会出错
	return SCB->ICSR & SCB_ICSR_PENDSVSET_Msk;
}

void Thread::OnSwitch()
{
	// 触发PendSV异常，引发上下文切换
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

void Thread::OnInit()
{
    Interrupt.SetPriority(PendSV_IRQn, 0xFF);
}

// 每个线程结束时执行该方法，销毁线程
void Thread::OnEnd()
{
	//SmartIRQ irq;	// 关闭全局中断，确保销毁成功
	__disable_irq();

	Thread* th = Thread::Current;
	if(th) delete th;

	__enable_irq();	// 这里必须手工释放，否则会导致全局中断没有打开而造成无法调度

	while(1);
}

/*************************************************************************/
// 线程池任务型
class ThreadItem
{
private:
	Thread*	_thread;

public:
	Action	Callback;	// 委托
	void*	Param;		// 参数

	ThreadItem()
	{
		_thread = new Thread(OnWork);
	}

	~ThreadItem()
	{
		delete _thread;
		_thread = nullptr;
	}

	static void OnWork(void* param)
	{
		((ThreadItem*)param)->Work();
	}

	void Work()
	{
		while(true)
		{
			if(Callback) Callback(Param);

			_thread->Suspend();
		}
	}

	void Resume()
	{
		_thread->Resume();
	}
};

/*void QueueUserWorkItem(Func func)
{
}*/

void QueueUserWorkItem(Action func, void* param)
{
}
