#ifndef __Thread_H__
#define __Thread_H__

#include "Sys.h"

class Thread;

// 线程
class Thread : public LinkedNode<Thread>
{
private:
	bool CheckExpire();	// 检查Sleep是否过期

public:
	uint ID;		// 编号
	string Name;	// 名称

	uint* Stack;	// 栈底
	uint* StackTop;	// 栈顶
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
		Lowest,		// 最低优先级
		BelowNormal,// 略低
		Normal,		// 普通优先级
		AboveNormal,// 略高
		Highest		// 最高优先级
	} Priorities;
	Priorities Priority;	// 优先级

	Thread(Action callback, void* state = NULL, uint stackSize = 0x100);
	virtual ~Thread();

	void Start();
	void Stop();
	void Suspend();
	void Resume();

	ulong DelayExpire;		// 过期时间，单位微秒。睡眠的线程达到该时间后将恢复唤醒
	void Sleep(uint ms);	// 睡眠指定毫秒数。

	// 静态管理
private:
	static bool Inited;
	static uint g_ID;

	static Thread* Free;	// 自由线程队列，未得到时间片
	static Thread* Busy;	// 正在享受时间片的高优先级就绪队列

	static void Add(Thread* thread);
	static void Remove(Thread* thread);

	static int PrepareReady();		// 准备就绪队列

	static void OnTick();	// 系统滴答时钟定时调用该方法

	static void Init();
	static void Schedule();	// 系统线程调度开始

public:
	static Thread* Current;	// 正在执行的线程
	static byte Count;		// 线程个数

	static Thread* Idle;	// 空闲线程。最低优先级
	static void Switch();	// 切换线程，马上切换时间片给下一个线程
};

#endif

/*
两个队列，一个全部队列Free，一个就绪队列Busy。
时间片将在任务队列Busy之中的线程分配，低优先级的线程Free没有机会，除非就绪队列Busy为空，重建就绪队列。
*/
