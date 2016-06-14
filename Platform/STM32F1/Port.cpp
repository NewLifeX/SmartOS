#include "Sys.h"
#include "Port.h"

#include "Platform\stm32.h"

#if defined(STM32F1) || defined(STM32F4)
static const byte PORT_IRQns[] = {
    EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn, EXTI4_IRQn, // 5个基础的
    EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn,    // EXTI9_5
    EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn   // EXTI15_10
};
#elif defined(STM32F0) || defined(GD32F150)
static const byte PORT_IRQns[] = {
    EXTI0_1_IRQn, EXTI0_1_IRQn, // 基础
    EXTI2_3_IRQn, EXTI2_3_IRQn, // 基础
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn,
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn   // EXTI15_10
};
#endif

//_force_inline GPIO_TypeDef* IndexToGroup(byte index) { return ((GPIO_TypeDef *) (GPIOA_BASE + (index << 10))); }
//_force_inline byte GroupToIndex(GPIO_TypeDef* group) { return (byte)(((int)group - GPIOA_BASE) >> 10); }

/******************************** Port ********************************/

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port

void* Port::IndexToGroup(byte index) { return ((GPIO_TypeDef *) (GPIOA_BASE + (index << 10))); }

void Port::OnOpenClock(Pin pin, bool flag)
{
    int gi = pin >> 4;

	FunctionalState fs = flag ? ENABLE : DISABLE;
#if defined(STM32F0) || defined(GD32F150)
    RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOAEN << gi, fs);
#elif defined(STM32F1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA << gi, fs);
#elif defined(STM32F4)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA << gi, fs);
#endif
}

// 确定配置,确认用对象内部的参数进行初始化
void Port::OpenPin()
{
	GPIO_InitTypeDef gpio;
	// 特别要慎重，有些结构体成员可能因为没有初始化而酿成大错
	GPIO_StructInit(&gpio);

    OnOpen(&gpio);
    GPIO_Init((GPIO_TypeDef*)Group, &gpio);
}

void Port::OnOpen(void* param)
{
	auto gpio	= (GPIO_InitTypeDef*)param;
    gpio->GPIO_Pin = Mask;

#ifdef STM32F1
	// PA15/PB3/PB4 需要关闭JTAG
	switch(_Pin)
	{
		case PA15:
		case PB3:
		case PB4:
		{
			debug_printf("Close JTAG Pin P%c%d \r\n", _PIN_NAME(_Pin));

			// PA15是jtag接口中的一员 想要使用 必须开启remap
			RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
			GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
			break;
		}
	}
#endif
}

void Port::AFConfig(GPIO_AF GPIO_AF) const
{
#if defined(STM32F0) || defined(GD32F150) || defined(STM32F4)
	assert(Opened, "打开后才能配置AF");

	GPIO_PinAFConfig((GPIO_TypeDef*)Group, _PIN(_Pin), GPIO_AF);
#endif
}

bool Port::Read() const
{
	if(_Pin == P0) return false;

	return GPIO_ReadInputData((GPIO_TypeDef*)Group) & Mask;
}
#endif

/******************************** OutputPort ********************************/

// 输出端口
#define REGION_Output 1
#ifdef REGION_Output

void OutputPort::OpenPin(void* param)
{
#ifndef STM32F4
	assert(Speed == 2 || Speed == 10 || Speed == 50, "Speed");
#else
	assert(Speed == 2 || Speed == 25 || Speed == 50 || Speed == 100, "Speed");
#endif

	auto gpio	= (GPIO_InitTypeDef*)param;

	switch(Speed)
	{
		case 2:		gpio->GPIO_Speed = GPIO_Speed_2MHz;	break;
#ifndef STM32F4
		case 10:	gpio->GPIO_Speed = GPIO_Speed_10MHz;	break;
#else
		case 25:	gpio->GPIO_Speed = GPIO_Speed_25MHz;	break;
		case 100:	gpio->GPIO_Speed = GPIO_Speed_100MHz;break;
#endif
		case 50:	gpio->GPIO_Speed = GPIO_Speed_50MHz;	break;
	}

#ifdef STM32F1
	gpio->GPIO_Mode	= OpenDrain ? GPIO_Mode_Out_OD : GPIO_Mode_Out_PP;
#else
	gpio->GPIO_Mode	= GPIO_Mode_OUT;
	gpio->GPIO_OType	= OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;
#endif
}

bool OutputPort::Read() const
{
	if(Empty()) return false;

	// 转为bool时会转为0/1
	bool rs = GPIO_ReadOutputData((GPIO_TypeDef*)Group) & Mask;
	return rs ^ Invert;
}

bool OutputPort::ReadInput() const
{
	if(Empty()) return false;

	return Port::Read() ^ Invert;
}

void OutputPort::Write(bool value) const
{
	if(Empty()) return;

    if(value ^ Invert)
        GPIO_SetBits((GPIO_TypeDef*)Group, Mask);
    else
        GPIO_ResetBits((GPIO_TypeDef*)Group, Mask);
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

void AlternatePort::OpenPin(void* param)
{
	auto gpio	= (GPIO_InitTypeDef*)param;

#ifdef STM32F1
	gpio->GPIO_Mode	= OpenDrain ? GPIO_Mode_AF_OD : GPIO_Mode_AF_PP;
#else
	gpio->GPIO_Mode	= GPIO_Mode_AF;
	gpio->GPIO_OType	= OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;
#endif
}

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

void InputPort::OpenPin(void* param)
{
	auto gpio	= (GPIO_InitTypeDef*)param;

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

InputPort::Trigger GetTrigger(InputPort::Trigger mode, bool invert)
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
}

void InputPort::ClosePin()
{
	int idx = Bits2Index(Mask);
	SetEXIT(idx, false, GetTrigger(Mode, Invert));
	if(!IsOnlyExOfInt(this, idx))return;
	Interrupt.Deactivate(PORT_IRQns[idx]);
}

// 注册回调  及中断使能
void InputPort::OnRegister()
{
	byte gi = _Pin >> 4;
	int idx = Bits2Index(Mask);

    // 打开时钟，选择端口作为端口EXTI时钟线
#if defined(STM32F0) || defined(GD32F150) || defined(STM32F4)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(gi, idx);
#elif defined(STM32F1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_EXTILineConfig(gi, idx);
#endif

	SetEXIT(idx, true, GetTrigger(Mode, Invert));

    // 打开并设置EXTI中断为低优先级
    Interrupt.SetPriority(PORT_IRQns[idx], 1);

    Interrupt.Activate(PORT_IRQns[idx], EXTI_IRQHandler, this);
}

#endif

/******************************** AnalogInPort ********************************/

void AnalogInPort::OpenPin(void* param)
{
	auto gpio	= (GPIO_InitTypeDef*)param;

#ifdef STM32F1
	gpio->GPIO_Mode	= GPIO_Mode_AIN; //
#else
	gpio->GPIO_Mode	= GPIO_Mode_AN;
	//gpio->GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;
#endif
}
