#ifndef __Thread_H__
#define __Thread_H__

#include "Kernel\Sys.h"
#include "Core\LinkedList.h"

class Thread;

// 线程
class Thread : public LinkedNode<Thread>
{
private:
	bool CheckExpire();	// 检查Sleep是否过期
	void CheckStack();	// 检查栈是否溢出

public:
	uint ID;		// 编号
	cstring Name;	// 名称

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
	bool IsReady;	// 是否位于就绪队列

	typedef enum
	{
		Lowest,		// 最低优先级
		BelowNormal,// 略低
		Normal,		// 普通优先级
		AboveNormal,// 略高
		Highest		// 最高优先级
	} Priorities;
	Priorities Priority;	// 优先级

	Thread(Action callback, void* state = nullptr, uint stackSize = 0x200);
	virtual ~Thread();

	void Start();
	void Stop();
	void Suspend();
	void Resume();

	UInt64 DelayExpire;		// 过期时间，单位微秒。睡眠的线程达到该时间后将恢复唤醒
	void Sleep(uint ms);	// 睡眠指定毫秒数。

	// 静态管理
private:
	static bool Inited;		// 是否已初始化
	static uint g_ID;		// 全局线程ID
	static byte	_running;	// 就绪队列线程数
	static byte _sleeps;	// 正在睡眠的线程数

	static Thread* Free;	// 自由线程队列，未得到时间片
	static Thread* Busy;	// 正在享受时间片的高优先级就绪队列

	static void Add(Thread* thread);
	static void Remove(Thread* thread);

	static void OnTick();	// 系统滴答时钟定时调用该方法

	static void Init();
	static void OnInit();
	static void OnEnd();	// 每个线程结束时执行该方法，销毁线程

	static byte BuildReady();// 准备就绪队列

	static void Schedule();	// 系统线程调度开始
	static void OnSchedule();

public:
	static Thread* Current;	// 正在执行的线程
	static Thread* Idle;	// 空闲线程。最低优先级
	static Thread* Main;	// 主线程。略低优先级
	static byte Count;		// 线程个数
	static void Switch();	// 切换线程，马上切换时间片给下一个线程
	
private:
	static bool CheckPend();
	static void OnSwitch();
	
// 线程池
public:
	//static void QueueUserWorkItem(Func func);
	static void QueueUserWorkItem(Action func, void* param);
};

#endif

/*
SmartOS基于优先级的抢占式多线程调度
特性：
1，支持无限多个线程。线程的增多并不影响切换效率，仅影响创建和停止等效率
2，支持无限多个动态优先级。数字越大优先级越高，0为最低优先级，支持运行时动态修改
3，支持单独设定每个线程的栈大小。根据需要合理使用内存
4，自动检查线程栈空间溢出。设计时检查栈溢出，发布时忽略以提升性能
5，调度算法采用两个双向链表，其中一个最高优先级就绪队列，调度时根本不需要搜索
6，支持时间片调度。同等最高优先级线程共同分享时间片
7，线程完成后自行销毁。

设计思路：
1，两个队列，一个就绪队列Busy，包含所有最高优先级就绪线程，一个后备队列Free，包含所有非最高优先级线程和最高优先级非就绪线程。
2，多线程调度将在就绪队列Busy之中执行，低优先级的线程Free没有机会，除非就绪队列Busy为空，重建就绪队列。切换线程时只需要取Busy的下一个节点即可，如果为空则从头开始，非常高效
3，线程的创建、停止、挂起、恢复、睡眠都可能会导致重建就绪队列，以确保最高优先级的就绪线程得到优先调度
4，没有其它处于就绪状态的线程时，系统将CPU资源分配给空闲线程Idle
5，每个线程有自己的栈空间，多线程调度的关键就是在PendSV中断里面切换将要调度的线程栈，A线程被PendSV中断打断，然后在PendSV中断里面把栈换成B线程，这样子在退出PendSV中断时将会跑到B线程去执行。
6，切换线程Switch可以由用户调用，也可以由系统滴答时钟调用
*/
