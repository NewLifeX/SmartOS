#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include "Sys.h"

// 中断委托（中断号，参数）
typedef void (*InterruptCallback)(ushort num, void* param);

#ifdef STM32F1
    #define VectorySize 84
#elif defined(STM32F0)
    #define VectorySize 48
#elif defined(STM32F4)
    #define VectorySize (86 + 16 + 1)
#endif

// 中断管理类
class TInterrupt
{
public:
    InterruptCallback Vectors[VectorySize];      // 对外的中断向量表
    void* Params[VectorySize];       // 每一个中断向量对应的参数

    void Init();    // 初始化中断向量表
    //~TInterrupt();

    // 注册中断函数（中断号，函数，参数）
    bool Activate(short irq, InterruptCallback isr, void* param = NULL);
    // 解除中断注册
    bool Deactivate(short irq);
    // 开中断
    bool Enable(short irq);
    // 关中断
    bool Disable(short irq);

    // 是否开中断
    bool EnableState(short irq);
    // 是否挂起
    bool PendingState(short irq);

    // 设置优先级
    void SetPriority(short irq, uint priority = 1);
    // 获取优先级
    void GetPriority(short irq);

#ifdef STM32F1
    // 编码优先级
    uint EncodePriority (uint priorityGroup, uint preemptPriority, uint subPriority);
    // 解码优先级
    void DecodePriority (uint priority, uint priorityGroup, uint* pPreemptPriority, uint* pSubPriority);
#endif

    void GlobalEnable();	// 打开全局中断
    void GlobalDisable();	// 关闭全局中断
	bool GlobalState();		// 全局中断开关状态

	bool IsHandler();		// 是否在中断里面
};

// 全局中断类对象
extern TInterrupt Interrupt;
// 会在maim（）  之前进行初始化   没有构造函数...
// 初始化函数  Interrupt.Init（）  在 Sys.cpp 内 TSys::TSys() 中被调用
// <TSys::TSys()也是构造函数   Sys.TSys()函数 在main（）函数之前被执行>

// 智能IRQ，初始化时备份，销毁时还原
// SmartIRQ相当霸道，它直接关闭所有中断，再也没有别的任务可以跟当前任务争夺MCU
class SmartIRQ
{
public:
	SmartIRQ(bool enable = false);
	~SmartIRQ();

private:
	uint _state;
};

// 智能锁。初始化时锁定一个整数，销毁时解锁
class Lock
{
private:
	int* _ref;		// 被加锁的整数所在指针

public:
	bool Success;	// 是否成功加锁

	Lock(int& ref);
	~Lock();

	bool Wait(int us = -1);
};

extern "C"
{
	void FaultHandler(void);
	void UserHandler(void);
	void ShowFault(uint exception);
}

#if DEBUG
// 函数栈。
// 进入函数时压栈函数名，离开时弹出。便于异常时获取主线程调用列表
class TraceStack
{
public:
	TraceStack(string name);
	~TraceStack();

	static void Show();
};

#define TS(name) TraceStack __ts(name)

#else

#define TS(name) ((void)0)

#endif

#endif

/*
完全接管中断，在RAM中开辟中断向量表，做到随时可换。
由于中断向量表要求128对齐，这里多分配128字节，找到对齐点后给向量表使用

为了增强中断函数处理，我们使用_Vectors作为真正的中断向量表，全部使用OnHandler作为中断处理函数。
然后在OnHandler内部获取中断号，再调用Vectors中保存的用户委托，并向它传递中断号和参数。
*/
