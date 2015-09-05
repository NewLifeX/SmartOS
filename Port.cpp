#include "Port.h"

#if defined(STM32F1) || defined(STM32F4)
static const int PORT_IRQns[] = {
    EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn, EXTI4_IRQn, // 5个基础的
    EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn,    // EXTI9_5
    EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn   // EXTI15_10
};
#elif defined(STM32F0)
static const int PORT_IRQns[] = {
    EXTI0_1_IRQn, EXTI0_1_IRQn, // 基础
    EXTI2_3_IRQn, EXTI2_3_IRQn, // 基础
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn,
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn   // EXTI15_10
};
#endif

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port
Port::Port()
{
	_Pin	= P0;
	Group	= NULL;
	PinBit	= 0;
}

Port::~Port()
{
	Close();
}

// 单一引脚初始化
Port& Port::Set(Pin pin)
{
	// 如果引脚不变，则不做处理
	if(pin == _Pin) return *this;

	// 释放已有引脚的保护
	if(_Pin != P0) Close();

    _Pin = pin;
	if(_Pin != P0)
	{
		Group = IndexToGroup(pin >> 4);
		PinBit = 1 << (pin & 0x0F);
	}
	else
	{
		Group = NULL;
		PinBit = 0;
	}

	//if(_Pin != P0) Open();

	return *this;
}

bool Port::Empty() const
{
	if(_Pin != P0) return false;

	if(Group == NULL || PinBit == 0) return true;

	return false;
}

// 确定配置,确认用对象内部的参数进行初始化
bool Port::Open()
{
	if(_Pin == P0) return false;
	if(Opened) return true;

    // 先打开时钟才能配置
    int gi = _Pin >> 4;
#ifdef STM32F0
    RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOAEN << gi, ENABLE);
#elif defined(STM32F1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA << gi, ENABLE);
#elif defined(STM32F4)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA << gi, ENABLE);
#endif

#if DEBUG
	// 保护引脚
	Show();
	Reserve(_Pin, true);
#endif

	GPIO_InitTypeDef gpio;
	// 特别要慎重，有些结构体成员可能因为没有初始化而酿成大错
	GPIO_StructInit(&gpio);

    OnOpen(gpio);
    GPIO_Init(Group, &gpio);

#if DEBUG
	debug_printf("\r\n");
#endif

	Opened = true;

	return true;
}

void Port::Close()
{
	if(!Opened) return;
	if(_Pin == P0) return;

    // 先打开时钟才能配置
    int gi = _Pin >> 4;
#ifdef STM32F0
    RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOAEN << gi, DISABLE);
#elif defined(STM32F1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA << gi, DISABLE);
#elif defined(STM32F4)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA << gi, DISABLE);
#endif

#if DEBUG
	// 保护引脚
	Show();
	Reserve(_Pin, false);
	debug_printf("\r\n");
#endif

	Opened = false;
}

void Port::OnOpen(GPIO_InitTypeDef& gpio)
{
    gpio.GPIO_Pin = PinBit;

#ifdef STM32F1
	// PA15/PB3/PB4 需要关闭JTAG
	switch(_Pin)
	{
		case PA15:
		case PB3:
		case PB4:
		{
			debug_printf("关闭 JTAG 为 P%c%d", _PIN_NAME(_Pin));

			// PA15是jtag接口中的一员 想要使用 必须开启remap
			RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
			GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
			break;
		}
	}
#endif
}

#if defined(STM32F0) || defined(STM32F4)
void Port::AFConfig(byte GPIO_AF)
{
	GPIO_PinAFConfig(Group, _PIN(_Pin), GPIO_AF);
}
#endif

GPIO_TypeDef* Port::IndexToGroup(byte index) { return ((GPIO_TypeDef *) (GPIOA_BASE + (index << 10))); }
byte Port::GroupToIndex(GPIO_TypeDef* group) { return (byte)(((int)group - GPIOA_BASE) >> 10); }
#endif

// 端口引脚保护
#if DEBUG
static ushort Reserved[8];		// 引脚保留位，记录每个引脚是否已经被保留，禁止别的模块使用

// 保护引脚，别的功能要使用时将会报错。返回是否保护成功
bool Port::Reserve(Pin pin, bool flag)
{
	debug_printf("::");
    int port = pin >> 4, bit = 1 << (pin & 0x0F);
    if (flag) {
        if (Reserved[port] & bit) {
			// 增加针脚已经被保护的提示，很多地方调用ReservePin而不写日志，得到False后直接抛异常
			debug_printf("打开 P%c%d 失败！该引脚已被打开", _PIN_NAME(pin));
			return false; // already reserved
		}
        Reserved[port] |= bit;

		debug_printf("打开 P%c%d", _PIN_NAME(pin));
    } else {
        Reserved[port] &= ~bit;

#if defined(STM32F1)
		int config = 0;
		uint shift = (pin & 7) << 2; // 4 bits / pin
		uint mask = 0xF << shift; // 屏蔽掉其它位
		GPIO_TypeDef* port2 = IndexToGroup(port); // pointer to the actual port registers
		if (pin & 0x08) { // bit 8 - 15
			config = port2->CRH & mask;
		} else { // bit 0-7
			config = port2->CRL & mask;
		}

		config >>= shift;	// 移位到最右边
		config &= 0xF;
		debug_printf("关闭 P%c%d Config=0x%02x", _PIN_NAME(pin), config);
#else
		debug_printf("关闭 P%c%d", _PIN_NAME(pin));
#endif
	}

    return true;
}

// 引脚是否被保护
bool Port::IsBusy(Pin pin)
{
    int port = pin >> 4, sh = pin & 0x0F;
    return (Reserved[port] >> sh) & 1;
}
#endif

// 引脚配置
#define REGION_Config 1
#ifdef REGION_Config
void OutputPort::OnOpen(GPIO_InitTypeDef& gpio)
{
#ifndef STM32F4
	assert_param(Speed == 2 || Speed == 10 || Speed == 50);
#else
	assert_param(Speed == 2 || Speed == 25 || Speed == 50 || Speed == 100);
#endif

#if DEBUG
	debug_printf(" %dM", Speed);
	if(OpenDrain) debug_printf(" 开漏");
	if(Invert) debug_printf(" 倒置");
#endif

	Port::OnOpen(gpio);

	switch(Speed)
	{
		case 2: gpio.GPIO_Speed = GPIO_Speed_2MHz; break;
#ifndef STM32F4
		case 10: gpio.GPIO_Speed = GPIO_Speed_10MHz; break;
#else
		case 25: gpio.GPIO_Speed = GPIO_Speed_25MHz; break;
		case 100: gpio.GPIO_Speed = GPIO_Speed_100MHz; break;
#endif
		case 50: gpio.GPIO_Speed = GPIO_Speed_50MHz; break;
	}

#ifdef STM32F1
	gpio.GPIO_Mode = OpenDrain ? GPIO_Mode_Out_OD : GPIO_Mode_Out_PP;
#else
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_OType = OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;
#endif

	// 配置之前，需要根据倒置情况来设定初始状态，也就是在打开端口之前必须明确端口高低状态
	ushort dat = GPIO_ReadOutputData(Group);
	if(!Invert)
		dat &= ~PinBit;
	else
		dat |= PinBit;
	GPIO_Write(Group, dat);
}

void AlternatePort::OnOpen(GPIO_InitTypeDef& gpio)
{
	OutputPort::OnOpen(gpio);

#ifdef STM32F1
	gpio.GPIO_Mode = OpenDrain ? GPIO_Mode_AF_OD : GPIO_Mode_AF_PP;
#else
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_OType = OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;
#endif
}

void InputPort::OnOpen(GPIO_InitTypeDef& gpio)
{
#if DEBUG
	debug_printf(" 抖动=%dus", ShakeTime);
	if(Floating) debug_printf(" 浮空");
	if(Invert) debug_printf(" 倒置");
#endif

	Port::OnOpen(gpio);

#ifdef STM32F1
	if(Floating)
		gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	else if(PuPd == PuPd_UP)
		gpio.GPIO_Mode = GPIO_Mode_IPU;
	else if(PuPd == PuPd_DOWN)
		gpio.GPIO_Mode = GPIO_Mode_IPD; // 这里很不确定，需要根据实际进行调整
#else
	gpio.GPIO_Mode = GPIO_Mode_IN;
	//gpio.GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;
#endif
}

void AnalogInPort::OnOpen(GPIO_InitTypeDef& gpio)
{
	Port::OnOpen(gpio);

#ifdef STM32F1
	gpio.GPIO_Mode = GPIO_Mode_AIN; //
#else
	gpio.GPIO_Mode = GPIO_Mode_AN;
	//gpio.GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;
#endif
}
#endif

// 输出端口
#define REGION_Output 1
#ifdef REGION_Output
ushort OutputPort::ReadGroup()    // 整组读取
{
	return GPIO_ReadOutputData(Group);
}

bool OutputPort::Read()
{
	if(Empty()) return false;

	// 转为bool时会转为0/1
	bool rs = GPIO_ReadOutputData(Group) & PinBit;
	return rs ^ Invert;
}

bool OutputPort::ReadInput()
{
	if(Empty()) return false;

	bool rs = GPIO_ReadInputData(Group) & PinBit;
	return rs ^ Invert;
}

bool OutputPort::Read(Pin pin)
{
	GPIO_TypeDef* group = _GROUP(pin);
	return (group->IDR >> (pin & 0xF)) & 1;
}

void OutputPort::Write(bool value)
{
	if(Empty()) return;

    if(value ^ Invert)
        GPIO_SetBits(Group, PinBit);
    else
        GPIO_ResetBits(Group, PinBit);
}

void OutputPort::WriteGroup(ushort value)
{
	if(Empty()) return;

    GPIO_Write(Group, value);
}

void OutputPort::Up(uint ms)
{
	if(Empty()) return;

    Write(true);
	Sys.Sleep(ms);
    Write(false);
}

void OutputPort::Blink(uint times, uint ms)
{
	if(Empty()) return;

	bool flag = true;
    for(int i=0; i<times; i++)
	{
		Write(flag);
		flag = !flag;
		Sys.Sleep(ms);
	}
    Write(false);
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
#endif

// 输入端口
#define REGION_Input 1
#ifdef REGION_Input
/* 中断状态结构体 */
/* 一共16条中断线，意味着同一条线每一组只能有一个引脚使用中断 */
typedef struct TIntState
{
    Pin Pin;
    InputPort::IOReadHandler Handler;	// 委托事件
	void* Param;	// 事件参数，一般用来作为事件挂载者的对象，然后借助静态方法调用成员方法
    bool OldValue;

    uint ShakeTime;     // 抖动时间
    int Used;   // 被使用次数。对于前5行中断来说，这个只会是1，对于后面的中断线来说，可能多个
} IntState;

// 16条中断线
static IntState State[16];
static bool hasInitState = false;

void RegisterInput(int groupIndex, int pinIndex, InputPort::IOReadHandler handler);
void UnRegisterInput(int pinIndex);

InputPort::~InputPort()
{
    // 取消所有中断
    if(_Registed) Register(NULL);
}

ushort InputPort::ReadGroup()    // 整组读取
{
	return GPIO_ReadInputData(Group);
}

// 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
bool InputPort::Read()
{
	// 转为bool时会转为0/1
	bool rs = GPIO_ReadInputData(Group) & PinBit;
	return rs ^ Invert;
}

bool InputPort::Read(Pin pin)
{
	GPIO_TypeDef* group = _GROUP(pin);
	return (group->IDR >> (pin & 0xF)) & 1;
}

// 注册回调  及中断使能
void InputPort::Register(IOReadHandler handler, void* param)
{
    if(!PinBit) return;

    // 检查并初始化中断线数组
    if(!hasInitState)
    {
        for(int i=0; i<16; i++)
        {
            IntState* state = &State[i];
            state->Pin = P0;
            state->Handler = NULL;
            state->Used = 0;
        }
        hasInitState = true;
    }

    byte gi = _Pin >> 4;
    ushort n = PinBit;
    for(int i=0; i<16 && n!=0; i++)
    {
        // 如果设置了这一位，则注册事件
        if(n & 0x01)
        {
            // 注册中断事件
            if(handler)
            {
                IntState* state = &State[i];
                state->ShakeTime = ShakeTime;
                RegisterInput(gi, i, handler, param);
            }
            else
                UnRegisterInput(i);
        }
        n >>= 1;
    }

    _Registed = handler != NULL;
}

#define IT 1
#ifdef IT
void GPIO_ISR (int num)  // 0 <= num <= 15
{
	if(!hasInitState) return;

	IntState* state = State + num;
	if(!state) return;

	uint bit = 1 << num;
	bool value;
	//byte line = EXTI_Line0 << num;
	// 如果未指定委托，则不处理
	if(!state->Handler) return;

	// 默认20us抖动时间
	uint shakeTime = state->ShakeTime;

	do {
		EXTI->PR = bit;   // 重置挂起位
		value = InputPort::Read(state->Pin); // 获取引脚状态
		if(shakeTime > 0)
		{
			// 值必须有变动才触发
			if(value == state->OldValue) return;

			Sys.Delay(shakeTime); // 避免抖动
		}
	} while (EXTI->PR & bit); // 如果再次挂起则重复
	//EXTI_ClearITPendingBit(line);
	// 值必须有变动才触发
	if(shakeTime > 0 && value == state->OldValue) return;
	state->OldValue = value;
	if(state->Handler)
	{
		// 新值value为true，说明是上升，第二个参数是down，所以取非
		state->Handler(state->Pin, !value, state->Param);
	}
}

void EXTI_IRQHandler(ushort num, void* param)
{
#if defined(STM32F1) || defined(STM32F4)
	// EXTI0 - EXTI4
	if(num <= EXTI4_IRQn)
		GPIO_ISR(num - EXTI0_IRQn);
	else if(num == EXTI9_5_IRQn)
	{
		// EXTI5 - EXTI9
		uint pending = EXTI->PR & EXTI->IMR & 0x03E0; // pending bits 5..9
		int num = 5; pending >>= 5;
		do {
			if (pending & 1) GPIO_ISR(num);
			num++; pending >>= 1;
		} while (pending);
	}
	else if(num == EXTI15_10_IRQn)
	{
		// EXTI10 - EXTI15
		uint pending = EXTI->PR & EXTI->IMR & 0xFC00; // pending bits 10..15
		int num = 10; pending >>= 10;
		do {
			if (pending & 1) GPIO_ISR(num);
			num++; pending >>= 1;
		} while (pending);
	}
#elif defined(STM32F0)
	switch(num)
	{
		case EXTI0_1_IRQn:
		{
			uint pending = EXTI->PR & EXTI->IMR & 0x0003; // pending bits 0..1
			int num = 0; pending >>= 0;
			do {
				if (pending & 1) GPIO_ISR(num);
				num++; pending >>= 1;
			} while (pending);
			break;
		}
		case EXTI2_3_IRQn:
		{
			uint pending = EXTI->PR & EXTI->IMR & 0x000c; // pending bits 3..2
			int num = 2; pending >>= 2;
			do {
				if (pending & 1) GPIO_ISR(num);
				num++; pending >>= 1;
			} while (pending);
		}
		case EXTI4_15_IRQn:
		{
			uint pending = EXTI->PR & EXTI->IMR & 0xFFF0; // pending bits 4..15
			int num = 4; pending >>= 4;
			do {
				if (pending & 1) GPIO_ISR(num);
				num++; pending >>= 1;
			} while (pending);
		}
	}
#endif
}
#endif

void SetEXIT(int pinIndex, bool enable)
{
    /* 配置EXTI中断线 */
    EXTI_InitTypeDef ext;
    EXTI_StructInit(&ext);
    ext.EXTI_Line = EXTI_Line0 << pinIndex;
    ext.EXTI_Mode = EXTI_Mode_Interrupt;
    ext.EXTI_Trigger = EXTI_Trigger_Rising_Falling; // 上升沿下降沿触发
    ext.EXTI_LineCmd = enable ? ENABLE : DISABLE;
    EXTI_Init(&ext);
}

// 申请引脚中断托管
void InputPort::RegisterInput(int groupIndex, int pinIndex, IOReadHandler handler, void* param)
{
    IntState* state = &State[pinIndex];
    Pin pin = (Pin)((groupIndex << 4) + pinIndex);
    // 检查是否已经注册到别的引脚上
    if(state->Pin != pin && state->Pin != P0)
    {
#if DEBUG
        debug_printf("EXTI%d can't register to P%c%d, it has register to P%c%d\r\n", groupIndex, _PIN_NAME(pin), _PIN_NAME(state->Pin));
#endif
        return;
    }
    state->Pin = pin;
    state->Handler = handler;
	state->Param = param;
	state->OldValue = Read(pin); // 预先保存当前状态值，后面跳变时触发中断

    // 打开时钟，选择端口作为端口EXTI时钟线
#if defined(STM32F0) || defined(STM32F4)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(groupIndex, pinIndex);
#elif defined(STM32F1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_EXTILineConfig(groupIndex, pinIndex);
#endif

	SetEXIT(pinIndex, true);

    // 打开并设置EXTI中断为低优先级
    Interrupt.SetPriority(PORT_IRQns[pinIndex], 1);

    state->Used++;
    if(state->Used == 1)
    {
        Interrupt.Activate(PORT_IRQns[pinIndex], EXTI_IRQHandler, this);
    }
}

void InputPort::UnRegisterInput(int pinIndex)
{
    IntState* state = &State[pinIndex];
    // 取消注册
    state->Pin = P0;
    state->Handler = 0;

	SetEXIT(pinIndex, false);

    state->Used--;
    if(state->Used == 0)
    {
        Interrupt.Deactivate(PORT_IRQns[pinIndex]);
    }
}
#endif
