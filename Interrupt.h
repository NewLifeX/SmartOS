#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include "Sys.h"

// 中断委托（中断号，参数）
typedef void (*InterruptCallback)(ushort num, void* param);

#ifdef STM32F10X
    #define VectorySize 84
#elif STM32F0XX
    #define VectorySize 48
#endif

// 中断管理类
class TInterrupt
{
private:
    uint Vectors[VectorySize];      // 对外的中断向量表
    uint Params[VectorySize];       // 每一个中断向量对应的参数

    static void OnHandler();

public:
    void Init();    // 初始化中断向量表
    virtual ~TInterrupt();

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
    void SetPriority(short irq, uint priority);
    // 获取优先级
    void GetPriority(short irq);
#ifdef STM32F10X
    // 编码优先级
    uint EncodePriority (uint priorityGroup, uint preemptPriority, uint subPriority);
    // 解码优先级
    void DecodePriority (uint priority, uint priorityGroup, uint* pPreemptPriority, uint* pSubPriority);
#endif

    void GlobalEnable();	// 打开全局中断
    void GlobalDisable();	// 关闭全局中断
	bool GlobalState();		// 全局中断开关状态
};

// 全局中断类对象
extern TInterrupt Interrupt;
// 会在maim（）  之前进行初始化   没有构造函数...
// 初始化函数  Interrupt.Init（）  在 Sys.cpp 内 TSys::TSys() 中被调用
// <TSys::TSys()也是构造函数   Sys.TSys()函数 在main（）函数之前被执行>

// 智能IRQ，初始化时备份，销毁时还原
class SmartIRQ
{
public:
	SmartIRQ(bool enable = false)
	{
		_state = __get_PRIMASK();
		if(enable)
			__enable_irq();
		else
			__disable_irq();
	}

	~SmartIRQ()
	{
		__set_PRIMASK(_state);
	}
	
private:
	uint _state;
};

#endif
