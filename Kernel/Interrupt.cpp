#include "Sys.h"

#include "Interrupt.h"

TInterrupt Interrupt;

//#define IS_IRQ(irq) (irq >= -16 && irq <= VectorySize - 16)

void TInterrupt::Init() const
{
    OnInit();
}

/*TInterrupt::~TInterrupt()
{
	// 恢复中断向量表
#if defined(STM32F1) || defined(STM32F4)
	NVIC_SetVectorTable(NVIC_VectTab_FLASH, 0);
#else
    SYSCFG_MemoryRemapConfig(SYSCFG_MemoryRemap_Flash);
#endif
}*/

bool TInterrupt::Activate(short irq, InterruptCallback isr, void* param)
{
    short irq2 = irq + 16; // exception = irq + 16
    Vectors[irq2] = isr;
    Params[irq2] = param;

    return OnActivate(irq);
}

bool TInterrupt::Deactivate(short irq)
{
    short irq2 = irq + 16; // exception = irq + 16
    Vectors[irq2] = 0;
    Params[irq2] = 0;

    return OnDeactivate(irq);
}

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

void TInterrupt::Process(uint num) const
{
	auto& inter	= Interrupt;
	//assert_param(num < VectorySize);
	//assert_param(Interrupt.Vectors[num]);
	if(!inter.Vectors[num]) return;

	// 内存检查
#if DEBUG
	Sys.CheckMemory();
#endif

    // 找到应用层中断委托并调用
    auto isr	= (InterruptCallback)inter.Vectors[num];
    void* param	= (void*)inter.Params[num];
    isr(num - 16, param);
}

// 智能IRQ，初始化时备份，销毁时还原
SmartIRQ::SmartIRQ(bool enable)
{
	_state = TInterrupt::GlobalState();
	if(enable)
		TInterrupt::GlobalEnable();
	else
		TInterrupt::GlobalDisable();
}

SmartIRQ::~SmartIRQ()
{
	//__set_PRIMASK(_state);
	if(_state)
		TInterrupt::GlobalDisable();
	else
		TInterrupt::GlobalEnable();
}

#pragma arm section code

/*================================ 锁 ================================*/

#include "Time.h"

// 智能锁。初始化时锁定一个整数，销毁时解锁
Lock::Lock(int& ref)
{
	Success = false;
	if(ref > 0) return;

	// 加全局锁以后再修改引用
	SmartIRQ irq;
	// 再次判断，DoubleLock双锁结构，避免小概率冲突
	if(ref > 0) return;

	_ref = &ref;
	ref++;
	Success = true;
}

Lock::~Lock()
{
	if(Success)
	{
		SmartIRQ irq;
		(*_ref)--;
	}
}

bool Lock::Wait(int us)
{
	// 可能已经进入成功
	if(Success) return true;

	int& ref = *_ref;
	// 等待超时时间
	TimeWheel tw(0, 0, us);
	tw.Sleep = 1;
	while(ref > 0)
	{
		// 延迟一下，释放CPU使用权
		//Sys.Sleep(1);
		if(tw.Expired()) return false;
	}

	// 加全局锁以后再修改引用
	SmartIRQ irq;
	// 再次判断，DoubleLock双锁结构，避免小概率冲突
	if(ref > 0) return false;

	ref++;
	Success = true;

	return true;
}

/*================================ 跟踪栈 ================================*/

#if DEBUG

// 使用字符串指针的指针，因为引用的都是字符串常量，不需要拷贝和分配空间
static cstring* _TS	= nullptr;
static int _TS_Len	= 0;

TraceStack::TraceStack(cstring name)
{
	// 字符串指针的数组
	static cstring __ts[16];
	_TS	= __ts;

	//_TS->Push(name);
	if(_TS_Len < 16) _TS[_TS_Len++]	= name;
}

TraceStack::~TraceStack()
{
	// 清空最后一个项目，避免误判
	//if(_TS_Len > 0) _TS[--_TS_Len]	= "";
	_TS_Len--;
}

void TraceStack::Show()
{
	debug_printf("TraceStack::Show:\r\n");
	if(_TS)
	{
		for(int i=_TS_Len - 1; i>=0; i--)
		{
			debug_printf("\t<=%s \r\n", _TS[i]);
		}
	}
}

#endif
