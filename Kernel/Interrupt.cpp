#include "Kernel\Sys.h"

#include "Interrupt.h"

extern InterruptCallback Vectors[];      // 对外的中断向量表
extern void* Params[];       // 每一个中断向量对应的参数

TInterrupt Interrupt;

void TInterrupt::Init() const
{
    OnInit();
}

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

// 关键性代码，放到开头
INROOT void TInterrupt::Process(uint num) const
{
	//auto& inter	= Interrupt;
	if(!Vectors[num]) return;

	// 内存检查
#if DEBUG
	Sys.CheckMemory();
#endif

    // 找到应用层中断委托并调用
    auto isr	= (InterruptCallback)Vectors[num];
    void* param	= (void*)Params[num];
    isr(num - 16, param);
}

// 系统挂起
void TInterrupt::Halt()
{
#if DEBUG
	TraceStack::Show();

	//auto sp	= SerialPort::GetMessagePort();
	//if(sp) sp->Flush();
#endif
	while(true);
}

/******************************** SmartIRQ ********************************/

// 智能IRQ，初始化时备份，销毁时还原
INROOT SmartIRQ::SmartIRQ(bool enable)
{
	_state = TInterrupt::GlobalState();
	if(enable)
		TInterrupt::GlobalEnable();
	else
		TInterrupt::GlobalDisable();
}

INROOT SmartIRQ::~SmartIRQ()
{
	//__set_PRIMASK(_state);
	if(_state)
		TInterrupt::GlobalDisable();
	else
		TInterrupt::GlobalEnable();
}

/******************************** Lock ********************************/

#include "TTime.h"

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

bool Lock::Wait(int ms)
{
	// 可能已经进入成功
	if(Success) return true;

	int& ref = *_ref;
	// 等待超时时间
	TimeWheel tw(ms);
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
