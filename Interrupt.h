#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include "Sys.h"

// 中断委托（中断号，参数）
typedef void (*InterruptCallback)(ushort num, void* param);

// 中断管理类
class TInterrupt
{
private:
    uint _mem[84 + 128 / 4];  // 中断向量表要求128对齐，这里多分配128字节，找到对齐点后给向量表使用
    uint* _Vectors; // 真正的中断向量表
    uint Vectors[84];   // 对外的中断向量表
    uint Params[84];    // 每一个中断向量对应的参数

    static void OnHandler();

public:
    void Init();    // 初始化中断向量表

    bool Activate(short irq, InterruptCallback isr, void* param = NULL);
    bool Deactivate(short irq);
    bool Enable(short irq);
    bool Disable(short irq);

    bool EnableState(short irq);
    bool PendingState(short irq);

    void SetPriority(short irq, uint priority);
    void GetPriority(short irq);
    uint EncodePriority (uint priorityGroup, uint preemptPriority, uint subPriority);
    void DecodePriority (uint priority, uint priorityGroup, uint* pPreemptPriority, uint* pSubPriority);
};

extern TInterrupt Interrupt;

#endif
