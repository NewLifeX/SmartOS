#include "Sys.h"
#include "Kernel\Interrupt.h"
#include "Device\Port.h"

#include "Platform\stm32.h"

/******************************** Port ********************************/

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port

// 分组时钟
static byte _GroupClock[10];

extern void OpenPeriphClock(Pin pin, bool flag);

static void OpenClock(Pin pin, bool flag)
{
    int gi = pin >> 4;

	if(flag)
	{
		// 增加计数，首次打开时钟
		if(_GroupClock[gi]++) return;
	}
	else
	{
		// 减少计数，最后一次关闭时钟
		if(_GroupClock[gi]-- > 1) return;
	}

	OpenPeriphClock(pin, flag);
}

// 确定配置,确认用对象内部的参数进行初始化
void Port::Opening()
{
    // 先打开时钟才能配置
	OpenClock(_Pin, true);

	GPIO_InitTypeDef gpio;
	// 特别要慎重，有些结构体成员可能因为没有初始化而酿成大错
	GPIO_StructInit(&gpio);

	State	= &gpio;
}

WEAK void Port_OnOpen() {}

void Port::OnOpen()
{
	auto gpio	= (GPIO_InitTypeDef*)State;
    gpio->GPIO_Pin = 1 << (_Pin & 0x0F);

	Port_OnOpen();
}

void Port::OnClose()
{
	// 不能随便关闭时钟，否则可能会影响别的引脚
	OpenClock(_Pin, false);

	delete (GPIO_InitTypeDef*)State;
	State	= nullptr;
}

bool Port::Read() const
{
	if(_Pin == P0) return false;

	auto gp	= IndexToGroup(_Pin >> 4);
	ushort ms	= 1 << (_Pin & 0x0F);
	return GPIO_ReadInputData(gp) & ms;
}
#endif

/******************************** OutputPort ********************************/

// 输出端口
#define REGION_Output 1
#ifdef REGION_Output

bool OutputPort::Read() const
{
	if(Empty()) return false;

	auto gp	= IndexToGroup(_Pin >> 4);
	ushort ms	= 1 << (_Pin & 0x0F);
	// 转为bool时会转为0/1
	bool rs = GPIO_ReadOutputData(gp) & ms;
	return rs ^ Invert;
}

void OutputPort::Write(bool value) const
{
	if(Empty()) return;

	auto gi	= IndexToGroup(_Pin >> 4);
	ushort ms	= 1 << (_Pin & 0x0F);
    if(value ^ Invert)
        GPIO_SetBits(gi, ms);
    else
        GPIO_ResetBits(gi, ms);
}

// 设置端口状态
void OutputPort::Write(Pin pin, bool value)
{
	if(pin == P0) return;

    if(value)
        GPIO_SetBits(_GROUP(pin), _PORT(pin));
    else
        GPIO_ResetBits(_GROUP(pin), _PORT(pin));
}

/******************************** AlternatePort ********************************/

#endif

/******************************** InputPort ********************************/

// 输入端口
#define REGION_Input 1
#ifdef REGION_Input
/* 中断状态结构体 */
/* 一共16条中断线，意味着同一条线每一组只能有一个引脚使用中断 */
typedef struct TIntState
{
    InputPort*	Port;
} IntState;

// 16条中断线
static IntState States[16];
static bool hasInitState = false;

int Bits2Index(ushort value)
{
    for(int i=0; i<16; i++)
    {
        if(value & 0x01) return i;
        value >>= 1;
    }

	return -1;
}

#define IT 1
#ifdef IT
void GPIO_ISR(int num)  // 0 <= num <= 15
{
	if(!hasInitState) return;

	auto st = &States[num];
	// 如果未指定委托，则不处理
	if(!st || !st->Port) return;

	uint bit	= 1 << num;
	bool value;
	do {
		EXTI->PR	= bit;   // 重置挂起位
		value	= st->Port->Read(); // 获取引脚状态
	} while (EXTI->PR & bit); // 如果再次挂起则重复
	// Read的时候已经计算倒置，这里不必重复计算
	st->Port->OnPress(value);
}

void EXTI_IRQHandler(ushort num, void* param)
{
#if defined(STM32F1) || defined(STM32F4)
	// EXTI0 - EXTI4
	if(num <= EXTI4_IRQn)
		GPIO_ISR(num - EXTI0_IRQn);
	else
#endif
	{
		uint pending = EXTI->PR & EXTI->IMR;
		for(int i=0; i < 16 && pending != 0; i++, pending >>= 1)
		{
			if (pending & 1) GPIO_ISR(i);
		}
	}
}
#endif

void SetEXIT(int pinIndex, bool enable, InputPort::Trigger mode)
{
    /* 配置EXTI中断线 */
    EXTI_InitTypeDef ext;
    EXTI_StructInit(&ext);
    ext.EXTI_Line		= EXTI_Line0 << pinIndex;
    ext.EXTI_Mode		= EXTI_Mode_Interrupt;
	if(mode == InputPort::Rising)
		ext.EXTI_Trigger	= EXTI_Trigger_Rising; // 上升沿触发
	else if(mode == InputPort::Falling)
		ext.EXTI_Trigger	= EXTI_Trigger_Falling; // 下降沿触发
	else
		ext.EXTI_Trigger	= EXTI_Trigger_Rising_Falling; // 上升沿下降沿触发

	ext.EXTI_LineCmd	= enable ? ENABLE : DISABLE;
    EXTI_Init(&ext);
}

void InputPort::OpenPin()
{
	auto gpio	= (GPIO_InitTypeDef*)State;

#ifdef STM32F1
	if(Floating)
		gpio->GPIO_Mode = GPIO_Mode_IN_FLOATING;
	else if(Pull == UP)
		gpio->GPIO_Mode = GPIO_Mode_IPU;
	else if(Pull == DOWN)
		gpio->GPIO_Mode = GPIO_Mode_IPD; // 这里很不确定，需要根据实际进行调整
#else
	gpio->GPIO_Mode = GPIO_Mode_IN;
	//gpio->GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;
#endif

    GPIO_Init(IndexToGroup(_Pin >> 4), gpio);
}

// 是否独享中断号
bool IsOnlyExOfInt(const InputPort* pt, int idx)
{
	int s=0, e=0;
#if defined(STM32F1) || defined(STM32F4)
	if(idx <= 4) return true;

	if(idx <= 9)
	{
		s	= 5;
		e	= 9;
	}
	else if(idx <= 15)
	{
		s	= 10;
		e	= 15;
	}
#elif defined(STM32F0) || defined(GD32F150)
	if(idx <= 1)
	{
		s	= 0;
		e	= 1;
	}
	else if(idx <= 3)
	{
		s	= 2;
		e	= 3;
	}
	else if(idx <= 15)
	{
		s	= 4;
		e	= 15;
	}
#endif
	for(int i = s; i <= e; i++)
		if(States[i].Port != nullptr && States[i].Port != pt) return false;

	return true;
}

/*InputPort::Trigger GetTrigger(InputPort::Trigger mode, bool invert)
{
	if(invert && mode != InputPort::Both)
	{
		// 把上升沿下降沿换过来
		if(mode == InputPort::Rising)
			mode	= InputPort::Falling;
		else if(mode == InputPort::Falling)
			mode	= InputPort::Rising;
	}

	return mode;
}*/

void InputPort::ClosePin()
{
	int idx = Bits2Index(1 << (_Pin & 0x0F));

    auto st = &States[idx];
	if(st->Port == this)
	{
		st->Port = nullptr;

		SetEXIT(idx, false, InputPort::Both);
		if(!IsOnlyExOfInt(this, idx))return;
		Interrupt.Deactivate(PORT_IRQns[idx]);
	}
}

// 注册回调  及中断使能
bool InputPort::OnRegister()
{
    // 检查并初始化中断线数组
    if(!hasInitState)
    {
        for(int i=0; i<16; i++)
        {
            States[i].Port	= nullptr;
        }
        hasInitState = true;
    }

	byte gi = _Pin >> 4;
	int idx = Bits2Index(1 << (_Pin & 0x0F));
	auto st = &States[idx];

	auto port	= st->Port;
    // 检查是否已经注册到别的引脚上
    if(port != this && port != nullptr)
    {
#if DEBUG
        debug_printf("中断线EXTI%d 不能注册到 P%c%d, 它已经注册到 P%c%d\r\n", gi, _PIN_NAME(_Pin), _PIN_NAME(port->_Pin));
#endif

		// 将来可能修改设计，即使注册失败，也可以开启一个短时间的定时任务，来替代中断输入
        return false;
    }
    st->Port		= this;

	//byte gi = _Pin >> 4;
	//int idx = Bits2Index(1 << (_Pin & 0x0F));

    // 打开时钟，选择端口作为端口EXTI时钟线
#if defined(STM32F0) || defined(GD32F150) || defined(STM32F4)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(gi, idx);
#elif defined(STM32F1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_EXTILineConfig(gi, idx);
#endif

	SetEXIT(idx, true, InputPort::Both);

    // 打开并设置EXTI中断为低优先级
    Interrupt.SetPriority(PORT_IRQns[idx], 1);

    Interrupt.Activate(PORT_IRQns[idx], EXTI_IRQHandler, this);

	return true;
}

#endif

/******************************** AnalogInPort ********************************/

void AnalogInPort::OpenPin()
{
	auto gpio	= (GPIO_InitTypeDef*)State;

#ifdef STM32F1
	gpio->GPIO_Mode	= GPIO_Mode_AIN; //
#else
	gpio->GPIO_Mode	= GPIO_Mode_AN;
	//gpio->GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;
#endif

    GPIO_Init(IndexToGroup(_Pin >> 4), gpio);
}
