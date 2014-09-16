#include "Thread.h"

Thread::Thread(Action callback, void* state, uint stackSize)
{
	if(!Inited) Init();

	StackSize = stackSize;
	// 栈以4字节来操作
	stackSize >>= 2;
	// 从堆里面动态分配一块空间作为线程栈空间
	uint* p = new uint[stackSize];
	Stack = p;
	StackTop = Stack + stackSize;

	// 默认状态就绪
	State = Ready;
	// 默认普通优先级
	Priority = Normal;

	Next = NULL;
	Prev = NULL;

	uint* top = StackTop;
    uint* stk = StackTop;          /* Load stack pointer                                 */
                                   /* Registers stacked as if auto-saved on exception    */
    *(stk)    = (uint)0x01000000L; /* xPSR                                               */
    *(--stk)  = (uint)callback;    /* Entry Point                                        */
    *(--stk)  = (uint)0xFFFFFFFEL; /* R14 (LR) (init value will cause fault if ever used)*/
    *(--stk)  = (uint)0x12121212L; /* R12                                                */
    *(--stk)  = (uint)0x03030303L; /* R3                                                 */
    *(--stk)  = (uint)0x02020202L; /* R2                                                 */
    *(--stk)  = (uint)0x01010101L; /* R1                                                 */
    *(--stk)  = (uint)state;       /* R0 : argument                                      */
                                   /* Remaining registers saved on process stack         */
    *(--stk)  = (uint)0x11111111L; /* R11                                                */
    *(--stk)  = (uint)0x10101010L; /* R10                                                */
    *(--stk)  = (uint)0x09090909L; /* R9                                                 */
    *(--stk)  = (uint)0x08080808L; /* R8                                                 */
    *(--stk)  = (uint)0x07070707L; /* R7                                                 */
    *(--stk)  = (uint)0x06060606L; /* R6                                                 */
    *(--stk)  = (uint)0x05050505L; /* R5                                                 */
    *(--stk)  = (uint)0x04040404L; /* R4                                                 */

	// 分配的栈至少要够自己用
	//assert_param(stk >= Stack);
	if(stk < p) debug_printf("StackSize must >= 0x%02x\r\n", (top - stk) << 2);

	Stack = stk;
}

Thread::~Thread()
{
	if(State != Stopped) Stop();

	Stack = StackTop - (StackSize >> 2);
	if(Stack) delete Stack;
	Stack = NULL;
}

void Thread::Start()
{
	assert_param(State == Ready || State == Stopped);

	State = Ready;

	Add(this);		// 进入线程队列
}

void Thread::Stop()
{
	assert_param(State == Running || State == Suspended);

	// 退出线程队列
	// 虽然内部会调用重新调度，但是一般Stop由中断函数执行，具有较高优先级，不会被重新调度打断，可确保完成
	Remove(this);

	State = Stopped;
}

void Thread::Suspend()
{
	assert_param(State == Running || State == Ready);

	State = Suspended;

	if(Current == this) Schedule();
}

void Thread::Resume()
{
	assert_param(State == Suspended);

	State = Ready;
}


bool Thread::Inited = false;
Thread* Thread::Free = NULL;
Thread* Thread::Busy = NULL;
Thread* Thread::Current = NULL;
byte Thread::Count = 0;
Thread* Thread::Idle = NULL;
Action Thread::IdleHandler = NULL;

void Thread::Add(Thread* thread)
{
	assert_param(thread);

	SmartIRQ irq;

	// 如果任务列表为空，则直接作为开头
	if(!Free)
	{
		Free = thread;

		thread->Prev = NULL;
		thread->Next = NULL;
	}
	else
	{
		// 把线程加到列表最后面
		Thread* th = Free;
		while(th->Next != NULL) th = th->Next;

		th->Next = thread;
		thread->Prev = th;
		thread->Next = NULL;
	}

	Count++;

	// 如果优先级最高，重建就绪队列
	// 如果就绪队列为空，也重新调度
	if(Current && thread->Priority >= Current->Priority || Busy == NULL) PrepareReady();
}

void Thread::Remove(Thread* thread)
{
	assert_param(thread);

	SmartIRQ irq;

	// 如果其为头部，注意修改头部指针
	if(thread->Prev == NULL)
	{
		if(thread == Free)
			Free = thread->Next;
		else if(thread == Busy)
			Busy = thread->Next;
		else
			assert_param(false);	// 如果不是两个头部，不可能前一个节点为空
	}
	else
	{
		// 直接从链表中移除
		thread->Prev->Next = thread->Next;
		if(thread->Next)
			thread->Next->Prev = thread->Prev;
	}

	Count--;

	// 如果就绪队列为空，重新调度
	if(Busy == NULL) PrepareReady();
	// 如果在就绪队列，刚好是当前线程，则放弃时间片，马上重新调度
	if(thread == Current) Schedule();
}

// 准备最高优先级的就绪队列。时间片将在该队列中分配
int Thread::PrepareReady()
{
	SmartIRQ irq;

	// 为了让算法简单易懂，采用两次循环，一次获取最高优先级，第二次才构建就绪列表

	Priorities pri = Lowest;
	Thread* th;
	// 找出所有已就绪线程的最高优先级
	for(th = Free; th; th = th->Next)
	{
		if(th->State == Ready)
		{
			if(th->Priority > pri) pri = th->Priority;
		}
	}
	for(th = Busy; th; th = th->Next)
	{
		if(th->State == Ready || th->State == Running)
		{
			if(th->Priority > pri) pri = th->Priority;
		}
	}

	// 根据最高优先级重构线程就绪队列
	Thread* head = NULL;
	Thread* tail = NULL;
	int count = 0;
	for(th = Free; th; th = th->Next)
	{
		if(th->State == Ready)
		{
			if(th->Priority == pri)
			{
				// 如果是开头，则重置开头
				if(th == Free) Free = th->Next;

				// 从原队列出列
				if(th->Prev) th->Prev->Next = th->Next;
				if(th->Next) th->Next->Prev = th->Prev;

				// 加入队列
				if(!head)
				{
					head = tail = th;
				}
				else
				{
					tail->Next = th;
					th->Prev = tail;
					tail = th;
				}

				count++;
			}
		}
	}
	for(th = Busy; th; th = th->Next)
	{
		if(th->State == Ready || th->State == Running)
		{
			if(th->Priority == pri)
			{
				// 如果是开头，则重置开头
				if(th == Busy) Busy = th->Next;

				// 从原队列出列
				if(th->Prev) th->Prev->Next = th->Next;
				if(th->Next) th->Next->Prev = th->Prev;

				// 加入队列
				if(!head)
				{
					head = tail = th;
				}
				else
				{
					tail->Next = th;
					th->Prev = tail;
					tail = th;
				}

				count++;
			}
		}
	}

	// 原就绪队列节点回到自由队列
	if(Busy)
	{
		if(Free)
		{
			// 找到末尾
			th = Free;
			while(th->Next) th = th->Next;

			th->Next = Busy;
			Busy->Prev = th;
		}
		else
		{
			Free = Busy;
		}
	}

	// 新的就绪队列
	Busy = head;

	return count;
}

// 系统线程调度开始
void Thread::Schedule()
{
	SmartIRQ irq;

	// 使用双栈。每个线程有自己的栈，属于PSP，中断专用MSP
	if(__get_CONTROL() != 2)
	{
		// 设置PSP，使用双栈
		// 很多RTOS在这里设置PSP为0，然后这个函数最后是一个死循环。
		// 其实，PSP为0以后，这个函数就无法正常退出了，我们把它设置为MSP，确保函数正常退出，外部死循环
		__set_PSP(__get_MSP());
		__set_CONTROL(2);
		__ISB();
	}

	// 有些说法这里MSP要8字节对齐。有些霸道。
	//__set_MSP(__get_MSP() & 0xFFFFFFF8);

	// 触发PendSV异常，引发上下文切换
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

// 切换线程，马上切换时间片给下一个线程
void Thread::Switch()
{
	// 触发PendSV异常，引发上下文切换
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

__asm void SaveRegister(uint* p)
{
	//MOV     R0, __cpp(p)
	STM     R0, {R4-R11}
	BX		LR
}

__asm void LoadRegister(uint* p)
{
	;MOV     R0, =p
	LDM     R0, {R4-R11}
	BX		LR
}

__asm void SaveFPU(uint* p)
{
	VSTMFD	R0!, {d8 - d15}
	BX		LR
}

__asm void LoadFPU(uint* p)
{
	VLDMFD	R0!, {d8 - d15}
	BX		LR
}

__asm void CheckLR()
{
	ORR LR, LR, #0x04
	BX  LR
}

void Thread::PendSV_Handler(ushort num, void* param)
{
	SmartIRQ irq;
	register uint* p __ASM("r0");

	if(Current)
	{
		p = (uint*)__get_PSP();
		if(!p)
		{
#ifdef STM32F4
			//SaveFPU(p);
#endif
			// 保存现场 r4-11   r0-3 pc等都被中断时压栈过了
			p -= 8;	// 保护8个寄存器，32个字节
			Current->Stack = p;	// 备份当前sp
			SaveRegister(p);
			// 这个时候可以检查线程栈是否溢出
		}

		Current = Current->Next;
	}

	if(!Current)
	{
		if(!Busy) PrepareReady();
		Current = Busy;
	}

	if(Current)
	{
		// 恢复R4-11到新的进程栈中
		p = Current->Stack;
		LoadRegister(p);
		p += 8;
#ifdef STM32F4
		//LoadFPU(p);
#endif
		//__set_PSP((uint)(Current->Stack + 8));
		__set_PSP((uint)(p));

		CheckLR();
	}
}

extern "C"
{
	void PendSV_Handler()
	{
		Thread::PendSV_Handler(0, 0);
	}
}

void Idle_Handler(void* param) { while(1); }

void Thread::Init()
{
	Inited = true;

	Free = NULL;
	Busy = NULL;
	Current = NULL;

	if(!IdleHandler) IdleHandler = Idle_Handler;
	// 创建一个空闲线程，确保队列不为空
	Thread* idle = new Thread(IdleHandler, NULL, 0x3c);
	idle->Name = "Idle";
	idle->Priority = Lowest;
	idle->Start();
	Idle = idle;

    Interrupt.SetPriority(PendSV_IRQn, 0xFF);
	Interrupt.Activate(PendSV_IRQn, PendSV_Handler, NULL);
	
	Time.OnInterrupt = Switch;
}
