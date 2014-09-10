#include "Thread.h"

Thread::Thread(Action callback, void* state, uint stackSize)
{
	StackSize = stackSize;
	// 栈以4字节来操作
	stackSize >>= 2;
	// 从堆里面动态分配一块空间作为线程栈空间
	Stack = new uint[stackSize];
	StackTop = Stack + stackSize;

	// 默认状态就绪
	Status = 0;
	// 默认普通优先级
	Priority = 1;

	Next = NULL;
	Prev = NULL;

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

	Stack = stk;
}

Thread::~Thread()
{
	Stack = StackTop - (StackSize >> 2);
	if(Stack) delete Stack;
	Stack = NULL;
}

void Thread::Schedule()
{
	SmartIRQ irq;


}

void OSStart(Thread* thread)
{
	NVIC_SetPriority(PendSV_IRQn, 0xFF);
	//__set_PSP(0);
	__set_CONTROL(2);
	__ISB();

	// 记住当前栈底
	thread->Stack = (uint*)(__get_PSP() - (0x24 >> 2));
	thread->Status = 1;

	// 触发PendSV异常，引发上下文切换
	SCB->ICSR |= SCB_ICSR_PENDSVSET_Msk;

    __enable_irq();	// 打开中断
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

__asm void CheckLR()
{
	ORR LR, LR, #0x04
	BX  LR
}

void Thread::PendSV_Handler(void* param)
{
	__disable_irq();	// 关闭中断
    uint* p = (uint*)__get_PSP();
	if(!p)
	{
		// 保存现场 r4-11   r0-3 pc等都被中断时压栈过了
		p -= 8;	// 保护8个寄存器，32个字节
		SaveRegister(p);
		//__ASM volatile ("MOV R0, %0" : : "r" (p) : "R0" );
		//__ASM volatile ("STM R0, {R4-R11}" : : : "R0");
		/*__asm__("\
		MOV %%R0, #p\
		STM %%R0, {%%R4-%%R11}"\
		:\					// 输出参数
		:"m"(p)\			// 输入参数
		:"R0", "R4", "R11"\	// 寄存器约束
		);*/
		Current->Stack = p;	// 备份当前sp到任务控制块
	}

	Current = New;
	New = NULL;

	// 恢复R4-11到新的进程栈中
	p = Current->Stack;
	LoadRegister(p);
	p += 8;
	__set_PSP((uint)p);

	// Ensure exception return uses process stack
	/*__arm
	{
		ORR LR, LR, #0x04
	}*/
	//__ASM volatile ("ORR LR, LR, #0x04" : : : "lr" );
	CheckLR();

	__enable_irq();	// 打开中断
}

void Thread::Init()
{
	Head = NULL;
	Current = NULL;
	New = NULL;
}
