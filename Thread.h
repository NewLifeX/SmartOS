#ifndef __Thread_H__
#define __Thread_H__

#include "Sys.h"

class Thread;

// 线程
class Thread
{
private:
public:
	ushort ID;		// 编号
	string Name;	// 名称

	uint* StackTop;	// 栈顶
	uint* Stack;	// 栈底
	uint StackSize;	// 栈大小

	typedef enum
	{
		Ready = 0,	// 就绪状态，等待调度执行
		Running,	// 正在执行状态
		Suspended,	// 挂起状态
		Stopped		// 停止状态
	} States;

	States State;	// 状态
	
	typedef enum
	{
		Lowest,
		BelowNormal,
		Normal,
		AboveNormal,
		Highest
	} Priorities;
	Priorities Priority;	// 优先级

	Thread* Next;
	Thread* Prev;

	Thread(Action callback, void* state = NULL, uint stackSize = 0x100);
	virtual ~Thread();

	void Start();
	void Stop();
	void Suspend();
	void Resume();

	// 静态管理
private:
	static bool Inited;
	
	static Thread* Free;	// 自由线程队列，未得到时间片
	static Thread* Busy;	// 正在享受时间片的高优先级就绪队列
	
	static void Add(Thread* thread);
	static void Remove(Thread* thread);
	
	static int PrepareReady();		// 准备就绪队列

public:
	static Thread* Current;	// 正在执行的线程
	static byte Count;		// 线程个数
	
	static Thread* Idle;	// 空闲线程。最低优先级
	static Action  IdleHandler;	// 空闲线程委托

	static void Init();
	static void Schedule();	// 任务调度，马上切换时间片给下一个线程

	_force_inline static void PendSV_Handler(ushort num, void* param);
};

#endif

/*
两个队列，一个全部任务队列，一个就绪队列。
时间片将在任务队列之中的线程分配，低优先级的线程没有机会，除非就绪队列为空，重建就绪队列。
*/
