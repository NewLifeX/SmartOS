#include "Thread.h"

Thread::Thread(Action callback, void* state, uint stackSize)
{
	if(!Inited) Init();

	ID = ++g_ID;
	if(g_ID >= 0xFFFF) g_ID = 0;
	debug_printf("Thread::Create %d 0x%08x StackSize=0x%04x", ID, callback, stackSize);

	StackSize = stackSize;
	// 栈以4字节来操作
	stackSize >>= 2;
	// 从堆里面动态分配一块空间作为线程栈空间
	uint* p = new uint[stackSize];
	Stack = p;
	StackTop = Stack + stackSize;
	debug_printf(" Stack=(0x%08x, 0x%08x)", Stack, StackTop);
	debug_printf("\r\n");

	// 默认状态就绪
	State = Ready;
	// 默认普通优先级
	Priority = Normal;

	DelayExpire = 0;

	Next = NULL;
	Prev = NULL;

	uint* top = StackTop;
    uint* stk = StackTop;          // 加载栈指针
                                   // 中断时自动保存一部分寄存器

	#ifdef STM32F4
	*(--stk)  = (uint)0xaa;        // R?
	*(--stk)  = (uint)0xa;         // FPSCR
	*(--stk)  = (uint)0x15;        // S15
	*(--stk)  = (uint)0x14;        // S14
	*(--stk)  = (uint)0x13;        // S13
	*(--stk)  = (uint)0x12;        // S12
	*(--stk)  = (uint)0x11;        // S11
	*(--stk)  = (uint)0x10;        // S10
	*(--stk)  = (uint)0x9;         // S9
	*(--stk)  = (uint)0x8;         // S8
	*(--stk)  = (uint)0x7;         // S7
	*(--stk)  = (uint)0x6;         // S6
	*(--stk)  = (uint)0x5;         // S5
	*(--stk)  = (uint)0x4;         // S4
	*(--stk)  = (uint)0x3;         // S3
	*(--stk)  = (uint)0x2;         // S2
	*(--stk)  = (uint)0x1;         // S1
	*(--stk)  = (uint)0x0;         // S0
	#endif

    *(stk)    = (uint)0x01000000L; // xPSR
    *(--stk)  = (uint)callback;    // Entry Point
    *(--stk)  = (uint)0xFFFFFFFEL; // R14 (LR) (初始值如果用过将导致异常)
    *(--stk)  = (uint)0x12121212L; // R12
    *(--stk)  = (uint)0x03030303L; // R3
    *(--stk)  = (uint)0x02020202L; // R2
    *(--stk)  = (uint)0x01010101L; // R1
    *(--stk)  = (uint)state;       // R0

	#ifdef STM32F4
	// FPU 寄存器 s16 ~ s31
	*(--stk)  = (uint)0x31uL;      // S31
	*(--stk)  = (uint)0x30uL;      // S30
	*(--stk)  = (uint)0x29uL;      // S29
	*(--stk)  = (uint)0x28uL;      // S28
	*(--stk)  = (uint)0x27uL;      // S27
	*(--stk)  = (uint)0x26uL;      // S26
	*(--stk)  = (uint)0x25uL;      // S25
	*(--stk)  = (uint)0x24uL;      // S24
	*(--stk)  = (uint)0x23uL;      // S23
	*(--stk)  = (uint)0x22uL;      // S22
	*(--stk)  = (uint)0x21uL;      // S21
	*(--stk)  = (uint)0x20uL;      // S20
	*(--stk)  = (uint)0x19uL;      // S19
	*(--stk)  = (uint)0x18uL;      // S18
	*(--stk)  = (uint)0x17uL;      // S17
	*(--stk)  = (uint)0x16uL;      // S16
	#endif

                                   // 余下的寄存器将保存到线程栈
    *(--stk)  = (uint)0x11111111L; // R11
    *(--stk)  = (uint)0x10101010L; // R10
    *(--stk)  = (uint)0x09090909L; // R9
    *(--stk)  = (uint)0x08080808L; // R8
    *(--stk)  = (uint)0x07070707L; // R7
    *(--stk)  = (uint)0x06060606L; // R6
    *(--stk)  = (uint)0x05050505L; // R5
    *(--stk)  = (uint)0x04040404L; // R4

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

	debug_printf("Thread::Start %d %s Priority=%d\r\n", ID, Name, Priority);

	State = Ready;

	Add(this);		// 进入线程队列
}

void Thread::Stop()
{
	assert_param(State == Running || State == Suspended);

	debug_printf("Thread::Stop %d %s\r\n", ID, Name);

	// 退出线程队列
	// 虽然内部会调用重新调度，但是一般Stop由中断函数执行，具有较高优先级，不会被重新调度打断，可确保完成
	Remove(this);

	State = Stopped;
}

void Thread::Suspend()
{
	assert_param(State == Running || State == Ready);

	debug_printf("Thread::Suspend %d %s", ID, Name);
	if(DelayExpire > 0) debug_printf(" for Sleep %dms", (uint)(DelayExpire - Time.Current()) / 1000);
	debug_printf("\r\n");

	State = Suspended;

	// 修改了状态，需要重新整理就绪列表
	PrepareReady();
}

void Thread::Resume()
{
	assert_param(State == Suspended);

	debug_printf("Thread::Resume %d %s\r\n", ID, Name);

	State = Ready;
	DelayExpire = 0;

	// 修改了状态，需要重新整理就绪列表
	PrepareReady();
}

void Thread::Sleep(uint ms)
{
	DelayExpire = Time.Current() + ms * 1000;

	Suspend();
}

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
		thread->LinkAfter(Free->Last());
	}

	Count++;

	// 如果就绪队列为空或优先级更高，重建就绪队列
	if(!Busy || thread->Priority >= Busy->Priority) PrepareReady();
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
		thread->Unlink();
	}

	Count--;

	// 如果刚好是当前线程，则放弃时间片，重新调度。因为PendSV优先级的原因，不会马上调度
	if(thread == Current) Switch();
	// 如果就绪队列为空，重新调度
	if(Busy == NULL) PrepareReady();
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
				th->Unlink();

				// 加入队列
				if(!head)
					head = tail = th;
				else
					th->LinkAfter(tail);

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
				th->Unlink();

				// 加入队列
				if(!head)
					head = tail = th;
				else
					th->LinkAfter(tail);

				count++;
			}
		}
	}

	// 原就绪队列节点回到自由队列
	if(Busy)
	{
		if(Free)
			Busy->LinkAfter(Free->Last());
		else
			Free = Busy;
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
	//SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

// 检查Sleep是否过期
bool Thread::CheckExpire()
{
	if(State == Suspended && DelayExpire > 0 && DelayExpire <= Time.Current())
	{
		Resume();
		return true;
	}
	return false;
}

extern "C"
{
	uint* curStack = 0;
	uint* newStack = 0;

	__asm void PendSV_Handler()
	{
		IMPORT curStack
		IMPORT newStack

		CPSID   I                       // 关闭全局中断

		LDR		R2, =curStack
		LDR		R2, [R2]
		LDR		R3, =newStack
		LDR		R3, [R3]
		CMP		R2, R3
		BEQ		PendSV_End				// 相等则说明是同一个线程，不需要调度

		CBZ     R2, PendSV_NoSave		// 如果当前线程栈为空则不需要保存。实际上不可能

		MRS     R0, PSP
		#ifdef STM32F4
		VSTMFD	r0!, {d8 - d15} 		// 压入 FPU 寄存器 s16~s31
		#endif

		SUBS    R0, R0, #0x20           // 保存r4-11到线程栈   r0-3 pc等在中断时被压栈了
		STM     R0, {R4-R11}

		STR     R0, [R2]				// 备份当前sp到任务控制块

PendSV_NoSave							// 此时整个上下文已经被保存
		LDM     R3, {R4-R11}            // 从新的栈中恢复 r4-11
		ADDS    R0, R3, #0x20

		#ifdef STM32F4
		VLDMFD	r0!, {d8 - d15} 		// 弹出 FPU 寄存器 s16~s31
		#endif

		MSR     PSP, R0                 // 修改PSP
		ORR     LR, LR, #0x04           // 确保中断返回用户栈

PendSV_End
		CPSIE   I
		BX      LR                      // 中断返回将恢复上下文
	}
}

// 切换线程，马上切换时间片给下一个线程
void Thread::Switch()
{
	// 准备当前任务和新任务的栈
	curStack = Current->Stack;
	Current = Current->Next;
	if(!Current)
	{
		if(!Busy) PrepareReady();
		Current = Busy;
		assert_param(Current);
	}
	newStack = Current->Stack;

	// 触发PendSV异常，引发上下文切换
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

void Thread::OnTick()
{
	// 检查睡眠到期的线程
	bool flag = false;
	for(Thread* th = Free; th; th = th->Next)
	{
		flag |= th->CheckExpire();
	}
	if(flag) PrepareReady();

	Switch();
}

void Idle_Handler(void* param) { while(1); }

bool Thread::Inited = false;
uint Thread::g_ID = 0;
Thread* Thread::Free = NULL;
Thread* Thread::Busy = NULL;
Thread* Thread::Current = NULL;
byte Thread::Count = 0;
Thread* Thread::Idle = NULL;

void Thread::Init()
{
	debug_printf("\r\n");
	debug_printf("初始化抢占式调度内核...\r\n");

	Inited = true;

	Free = NULL;
	Busy = NULL;
	Current = NULL;

	// 创建一个空闲线程，确保队列不为空
#ifdef STM32F4
	Thread* idle = new Thread(Idle_Handler, NULL, 0xC4);
#else
	Thread* idle = new Thread(Idle_Handler, NULL, 0x3C);
#endif
	idle->Name = "Idle";
	idle->Priority = Lowest;
	idle->Start();
	Idle = idle;

	// 把main函数主线程作为Idle线程，这是一个不需要外部创建就可以使用的线程
	Current = Idle;

    Interrupt.SetPriority(PendSV_IRQn, 0xFF);
	//Interrupt.Activate(PendSV_IRQn, PendSV_Handler, NULL);

	Schedule();

	Time.OnInterrupt = OnTick;
}
