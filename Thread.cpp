#include "Thread.h"
#include "Task.h"

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

Thread::Thread(Action callback, void* state, uint stackSize)
{
	SmartIRQ irq;	// 关闭全局中断

	// 栈大小必须4字节对齐
	assert_param((stackSize & 0x03) == 0);

	if(!Inited) Init();

	ID = ++g_ID;
	if(g_ID >= 0xFFFF) g_ID = 0;
	debug_printf("Thread::Create %d 0x%08x StackSize=0x%04x", ID, callback, stackSize);

	Name = NULL;

	// 外部传入的stackSize参数只是用户可用的栈大小，这里还需要加上保存寄存器所需要的stk部分
	uint stkSize = STACK_Size;

	stackSize += stkSize;
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
	IsReady = false;
	// 默认普通优先级
	Priority = Normal;

	DelayExpire = 0;

	Next = NULL;
	Prev = NULL;

    uint* stk = StackTop;          // 加载栈指针
                                   // 中断时自动保存一部分寄存器

	#ifdef FPU
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
    //*(--stk)  = (uint)0xFFFFFFFEL; // R14 (LR) (初始值如果用过将导致异常)
    *(--stk)  = (uint)OnEnd; // R14 (LR)
    *(--stk)  = (uint)0x12121212L; // R12
    *(--stk)  = (uint)0x03030303L; // R3
    *(--stk)  = (uint)0x02020202L; // R2
    *(--stk)  = (uint)0x01010101L; // R1
    *(--stk)  = (uint)state;       // R0

	#ifdef FPU
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
	if(stk < p) debug_printf("StackSize must >= 0x%02x\r\n", (StackTop - stk) << 2);
	assert_param(stk >= Stack);

	Stack = stk;
}

Thread::~Thread()
{
	SmartIRQ irq;	// 关闭全局中断，确保销毁成功

	if(State != Stopped) Stop();

	Stack = StackTop - (StackSize >> 2);
	if(Stack) delete[] Stack;
	Stack = NULL;
	StackTop = NULL;
}

void Thread::Start()
{
	SmartIRQ irq;	// 关闭全局中断

	assert_param(State == Ready || State == Stopped);

	debug_printf("Thread::Start %d %s Priority=%d\r\n", ID, Name, Priority);

	State = Ready;

	Add(this);		// 进入线程队列
}

void Thread::Stop()
{
	SmartIRQ irq;	// 关闭全局中断

	// 任何状态都应该可以停止
	//assert_param(State == Running || State == Suspended);

	debug_printf("Thread::Stop %d %s\r\n", ID, Name);

	// 退出线程队列
	// 虽然内部会调用重新调度，但是一般Stop由中断函数执行，具有较高优先级，不会被重新调度打断，可确保完成
	Remove(this);

	State = Stopped;
}

void Thread::Suspend()
{
	SmartIRQ irq;	// 关闭全局中断

	assert_param(State == Running || State == Ready);

	debug_printf("Thread::Suspend %d %s\r\n", ID, Name);

	State = Suspended;

	if(IsReady)
	{
		// 修改了状态，需要重新整理就绪列表
		BuildReady();
		if(this == Current) Switch();
	}
}

void Thread::Resume()
{
	SmartIRQ irq;	// 关闭全局中断

	assert_param(State == Suspended);

	debug_printf("Thread::Resume %d %s\r\n", ID, Name);

	State = Ready;

	if(DelayExpire > 0) _sleeps--;
	DelayExpire = 0;

	// 修改了状态，需要重新整理就绪列表
	BuildReady();
	Switch();
}

void Thread::Sleep(uint ms)
{
	SmartIRQ irq;	// 关闭全局中断

	DelayExpire = Sys.Ms() + ms;

	assert_param(State == Running || State == Ready);

#if TH_DEBUG
	debug_printf("Thread::Sleep %d %s for %dms\r\n", ID, Name, ms);
#endif

	State = Suspended;
	_sleeps++;

	if(IsReady)
	{
		// 修改了状态，需要重新整理就绪列表
		BuildReady();
		if(this == Current) Switch();
	}
}

// 检查Sleep是否过期
bool Thread::CheckExpire()
{
	if(State == Suspended && DelayExpire > 0 && DelayExpire <= Sys.Ms())
	{
		//Resume();
		State = Ready;
		DelayExpire = 0;
		_sleeps--;

		return true;
	}
	return false;
}

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
	assert_param(stackBottom <= p);
#endif
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
	if(!Busy || thread->Priority >= Busy->Priority) BuildReady();
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
		{
			assert_param(false);	// 如果不是两个头部，不可能前一个节点为空
			return;
		}
	}

	// 直接从链表中移除
	thread->Unlink();

	Count--;

	// 如果刚好是当前线程，则放弃时间片，重新调度。因为PendSV优先级的原因，不会马上调度
	if(thread == Current) Switch();
	// 如果就绪队列为空，重新调度。这里其实不用操心，Switch里面会准备好Busy
	if(Busy == NULL) BuildReady();
}

// 查找最高优先级
byte FindMax(Thread* first, byte pri)
{
	for(Thread* th = first; th; th = th->Next)
	{
		if(th->State == Thread::Ready || th->State == Thread::Running)
		{
			if(th->Priority > pri) pri = th->Priority;
		}
	}
	return pri;
}

// 建立指定优先级的线程列表
byte BuildList(Thread*& list, Thread*& head, Thread*& tail, byte pri)
{
	byte count = 0;
	for(Thread* th = list; th; th = th->Next)
	{
		if(th->State == Thread::Ready || th->State == Thread::Running)
		{
			if(th->Priority == pri)
			{
				// 如果是开头，则重置开头
				if(th == list) list = th->Next;

				// 从原队列出列
				th->Unlink();

				// 加入队列
				if(!head)
					head = tail = th;
				else
					th->LinkAfter(tail);

				count++;

				th->IsReady = true;
				continue;
			}
		}
		th->IsReady = false;
	}
	return count;
}

// 准备最高优先级的就绪队列。时间片将在该队列中分配
byte Thread::BuildReady()
{
	SmartIRQ irq;

	// 为了让算法简单易懂，采用两次循环，一次获取最高优先级，第二次才构建就绪列表

	// 找出所有已就绪线程的最高优先级
	byte pri = Lowest;
	pri = FindMax(Free, pri);
	pri = FindMax(Busy, pri);

	// 根据最高优先级重构线程就绪队列
	Thread* head = NULL;
	Thread* tail = NULL;
	byte count = 0;
	count += BuildList(Free, head, tail, pri);
	count += BuildList(Busy, head, tail, pri);
	_running = count;

	// 原就绪队列余下节点回到自由队列
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

void OnSleep(uint ms)
{
	Thread* th = Thread::Current;
	if(th) th->Sleep(ms);
}

//byte Thread_PSP[STACK_SAVE_Size];

// 系统线程调度开始
void Thread::Schedule()
{
	if(!Inited) Init();

	//SmartIRQ irq;
	__disable_irq();

	//Sys.OnTick = OnTick;
	Sys.OnSleep = OnSleep;

	// 先切换好了才换栈，因为里面有很多层调用，不确定新栈空间是否足够
	Switch();

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

	debug_printf("开始调度%d个用户线程\r\n", Count - 1);

	__enable_irq();	// 这里必须手工释放，否则会导致全局中断没有打开而造成无法调度

	while(1);
}

extern "C"
{
	uint** curStack = 0;	// 当前线程栈的指针。需要保存线程栈，所以需要指针
	uint* newStack = 0;		// 新的线程栈

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
void Thread::Switch()
{
	SmartIRQ irq;	// 关闭全局中断

	// 如果有挂起的切换，则不再切换。否则切换时需要保存的栈会出错
	if(SCB->ICSR & SCB_ICSR_PENDSVSET_Msk) return;

	// 准备当前任务和新任务的栈
	curStack = 0;
	if(Current)
	{
		// 检查线程栈空间
		Current->CheckStack();

		curStack = &Current->Stack;

		// 下一个获得时间片的线程
		Current = Current->Next;
	}
	// 如果下一个线程为空
	if(!Current)
	{
		// 如果就绪队列也为空，则重建就绪队列
		if(!Busy) BuildReady();

		// 重新取就绪队列头部
		Current = Busy;
		if(!Current) debug_printf("没有可调度线程，可能是挂起或睡眠了Idle线程\r\n");
		assert_param(Current);
	}
	newStack = Current->Stack;

	// 如果栈相同，说明是同一个线程，不需要切换
	if(curStack != 0 && *curStack == newStack) return;

	// 检查新栈的xPSR
	//uint* psr = newStack + (STACK_Size >> 2) - 1;
	//if(*psr != 0x01000000L) debug_printf("可能出错 xPSR=0x%08x\r\n", *psr);

	// 触发PendSV异常，引发上下文切换
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;
}

void Thread::OnTick()
{
	if(_sleeps > 0)
	{
		// 检查睡眠到期的线程
		bool flag = false;
		for(Thread* th = Free; th; th = th->Next)
		{
			flag |= th->CheckExpire();
		}
		if(flag) BuildReady();
	}

	/*// 单一任务不调度
	if(_running > 1) Switch();*/
	Switch();
}

void Idle_Handler(void* param) { while(1); }
void Main_Handler(void* param) { Task::Scheduler()->Start(); while(1); }

bool Thread::Inited = false;
uint Thread::g_ID = 0;
byte Thread::_running = 0;
byte Thread::_sleeps = 0;
Thread* Thread::Free = NULL;
Thread* Thread::Busy = NULL;
Thread* Thread::Current = NULL;
byte Thread::Count = 0;
Thread* Thread::Idle = NULL;
Thread* Thread::Main = NULL;

void Thread::Init()
{
	SmartIRQ irq;	// 关闭全局中断

	debug_printf("\r\n");
	debug_printf("初始化抢占式多线程调度...\r\n");

	Inited = true;

	Free = NULL;
	Busy = NULL;
	Current = NULL;

	// 创建一个空闲线程，确保队列不为空
	Thread* idle = new Thread(Idle_Handler, NULL, 0);
	idle->Name = "Idle";
	idle->Priority = Lowest;
	idle->Start();
	Idle = idle;

	// 多线程调度与Sys定时调度联动，由多线程调度器的Main线程负责驱动Sys.Start实现传统定时任务。要小心线程栈溢出
	Thread* main = new Thread(Main_Handler, NULL, 0x400);
	main->Name = "Main";
	main->Priority = BelowNormal;
	main->Start();
	Main = main;

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
		_thread = NULL;
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
