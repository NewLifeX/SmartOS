#include "Kernel\Sys.h"
#include "Kernel\Interrupt.h"
#include "Device\Port.h"

#include "Platform\stm32.h"

static const byte PORT_IRQns[] = {
    EXTI0_1_IRQn, EXTI0_1_IRQn, // 基础
    EXTI2_3_IRQn, EXTI2_3_IRQn, // 基础
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn,
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn   // EXTI15_10
};

/******************************** Port ********************************/

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port

extern GPIO_TypeDef* IndexToGroup(byte index);

void OpenPeriphClock(Pin pin, bool flag)
{
    int gi = pin >> 4;
	FunctionalState fs = flag ? ENABLE : DISABLE;
    RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOAEN << gi, fs);
}

void Port::AFConfig(GPIO_AF GPIO_AF) const
{
	assert(Opened, "打开后才能配置AF");

	GPIO_PinAFConfig(IndexToGroup(_Pin >> 4), _Pin & 0x000F, GPIO_AF);
}
#endif

/******************************** OutputPort ********************************/

// 输出端口
#define REGION_Output 1
#ifdef REGION_Output

void OutputPort::OpenPin(void* param)
{
	assert(Speed == 2 || Speed == 10 || Speed == 50, "Speed");

	auto gpio	= (GPIO_InitTypeDef*)param;

	switch(Speed)
	{
		case 2:		gpio->GPIO_Speed = GPIO_Speed_2MHz;	break;
		case 10:	gpio->GPIO_Speed = GPIO_Speed_10MHz;	break;
		case 50:	gpio->GPIO_Speed = GPIO_Speed_50MHz;	break;
	}

	gpio->GPIO_Mode	= GPIO_Mode_OUT;
	gpio->GPIO_OType	= OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;

    GPIO_Init(IndexToGroup(_Pin >> 4), gpio);
}

/******************************** AlternatePort ********************************/

void AlternatePort::OpenPin(void* param)
{
	auto gpio	= (GPIO_InitTypeDef*)param;

	gpio->GPIO_Mode	= GPIO_Mode_AF;
	gpio->GPIO_OType	= OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;

    GPIO_Init(IndexToGroup(_Pin >> 4), gpio);
}

#endif

/******************************** InputPort ********************************/

// 输入端口
#define REGION_Input 1
#ifdef REGION_Input

extern int Bits2Index(ushort value);
extern bool InputPort_HasEXTI(int line, const InputPort& port);
extern void GPIO_ISR(int num);
extern void SetEXIT(int pinIndex, bool enable, InputPort::Trigger mode);

void InputPort::OpenPin(void* param)
{
	auto gpio	= (GPIO_InitTypeDef*)param;

	gpio->GPIO_Mode = GPIO_Mode_IN;
	//gpio->GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;

    GPIO_Init(IndexToGroup(_Pin >> 4), gpio);
}

// 是否独享中断号
static bool IsOnlyExOfInt(const InputPort& port, int idx)
{
	int s=0, e=0;
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
	for(int i = s; i <= e; i++)
		if(InputPort_HasEXTI(i, port)) return false;

	return true;
}

void InputPort_CloseEXTI(const InputPort& port)
{
	Pin pin	= port._Pin;
	int idx = Bits2Index(1 << (pin & 0x0F));

	SetEXIT(idx, false, InputPort::Both);
	if(!IsOnlyExOfInt(port, idx))return;
	Interrupt.Deactivate(PORT_IRQns[idx]);
}

void EXTI_IRQHandler(ushort num, void* param)
{
	uint pending = EXTI->PR & EXTI->IMR;
	for(int i=0; i < 16 && pending != 0; i++, pending >>= 1)
	{
		if (pending & 1) GPIO_ISR(i);
	}
}

void InputPort_OpenEXTI(InputPort& port)
{
	Pin pin	= port._Pin;
	byte gi = pin >> 4;
	int idx = Bits2Index(1 << (pin & 0x0F));

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(gi, idx);

	SetEXIT(idx, true, InputPort::Both);

    // 打开并设置EXTI中断为低优先级
    Interrupt.SetPriority(PORT_IRQns[idx], 1);

    Interrupt.Activate(PORT_IRQns[idx], EXTI_IRQHandler, &port);
}

#endif

/******************************** AnalogInPort ********************************/

void AnalogInPort::OpenPin(void* param)
{
	auto gpio	= (GPIO_InitTypeDef*)param;

	gpio->GPIO_Mode	= GPIO_Mode_AN;
	//gpio->GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;

    GPIO_Init(IndexToGroup(_Pin >> 4), gpio);
}
