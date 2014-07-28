#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__

#include "Sys.h"

// 中断管理类
class TInterrupt
{
private:
    uint _mem[84 + 128 / 4];  // 中断向量表要求128对齐，这里多分配128字节，找到对齐点后给向量表使用
    uint* Vectors;  // 中断向量表
    void OnHandler();

public:
    void Init();    // 初始化中断向量表

    bool Activate(int irq, Func isr);
    bool Deactivate(int irq);
    bool Enable(int irq);
    bool Disable(int irq);

    bool EnableState(int irq);
    bool PendingState(int irq);

    void SetPriority(int irq, uint priority);
    void GetPriority(int irq);
    uint EncodePriority (uint priorityGroup, uint preemptPriority, uint subPriority);
    void DecodePriority (uint priority, uint priorityGroup, uint* pPreemptPriority, uint* pSubPriority);
};

extern TInterrupt Interrupt;

#endif
