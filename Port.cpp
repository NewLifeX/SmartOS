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

/******************************** Port ********************************/

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port
Port::Port()
{
	_Pin	= P0;
	Group	= NULL;
	Mask	= 0;
	Opened	= false;
#if DEBUG
	Debug	= true;
#endif
}

#ifndef TINY
Port::~Port()
{
	Close();
}
#endif

String& Port::ToStr(String& str) const
{
	str.SetAt(0, 'P');
	if(_Pin == P0)
	{
		str.SetAt(1, '0');
	}
	else
	{
		str.SetAt(1, 'A' + (_Pin >> 4));
		str.Append(_Pin & 0x0F);
	}
	return str;
}

// 单一引脚初始化
Port& Port::Set(Pin pin)
{
	// 如果引脚不变，则不做处理
	if(pin == _Pin) return *this;

#ifndef TINY
	// 释放已有引脚的保护
	if(_Pin != P0) Close();
#endif

    _Pin = pin;
	if(_Pin != P0)
	{
		Group	= IndexToGroup(pin >> 4);
		Mask	= 1 << (pin & 0x0F);
	}
	else
	{
		Group	= NULL;
		Mask	= 0;
	}

	//if(_Pin != P0) Open();

	return *this;
}

bool Port::Empty() const
{
	if(_Pin != P0) return false;

	if(Group == NULL || Mask == 0) return true;

	return false;
}

void Port::Clear()
{
	Group	= NULL;
	_Pin	= P0;
	Mask	= 0;
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
	if(Debug) Show();
	Reserve(_Pin, true, Debug);
#endif

	GPIO_InitTypeDef gpio;
	// 特别要慎重，有些结构体成员可能因为没有初始化而酿成大错
	GPIO_StructInit(&gpio);

    OnOpen(gpio);
    GPIO_Init(Group, &gpio);

#if DEBUG
	if(Debug) debug_printf("\r\n");
#endif

	Opened = true;

	return true;
}

void Port::Close()
{
	if(!Opened) return;
	if(_Pin == P0) return;

	OnClose();

#if DEBUG
	// 保护引脚
	if(Debug) Show();
	Reserve(_Pin, false, Debug);
	if(Debug) debug_printf("\r\n");
#endif

	Opened = false;
}

void Port::OnClose()
{
    int gi = _Pin >> 4;
#ifdef STM32F0
    RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOAEN << gi, DISABLE);
#elif defined(STM32F1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA << gi, DISABLE);
#elif defined(STM32F4)
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA << gi, DISABLE);
#endif
}

void Port::OnOpen(GPIO_InitTypeDef& gpio)
{
    gpio.GPIO_Pin = Mask;

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
void Port::AFConfig(byte GPIO_AF) const
{
	GPIO_PinAFConfig(Group, _PIN(_Pin), GPIO_AF);
}
#endif

GPIO_TypeDef* Port::IndexToGroup(byte index) { return ((GPIO_TypeDef *) (GPIOA_BASE + (index << 10))); }
byte Port::GroupToIndex(GPIO_TypeDef* group) { return (byte)(((int)group - GPIOA_BASE) >> 10); }
#endif

// 端口引脚保护
#if DEBUG
// 引脚保留位，记录每个引脚是否已经被保留，禁止别的模块使用
// !!!注意，不能全零，否则可能会被当作zeroinit而不是copy，导致得不到初始化
static ushort Reserved[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF};

// 保护引脚，别的功能要使用时将会报错。返回是否保护成功
bool Port::Reserve(Pin pin, bool flag, bool debug)
{
	if(debug) debug_printf("::");
    int port = pin >> 4, bit = 1 << (pin & 0x0F);
    if (flag) {
        if (Reserved[port] & bit) {
			// 增加针脚已经被保护的提示，很多地方调用ReservePin而不写日志，得到False后直接抛异常
			debug_printf("打开 P%c%d 失败！该引脚已被打开", _PIN_NAME(pin));
			return false; // already reserved
		}
        Reserved[port] |= bit;

		if(debug) debug_printf("打开 P%c%d", _PIN_NAME(pin));
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
		if(debug) debug_printf("关闭 P%c%d Config=0x%02x", _PIN_NAME(pin), config);
#else
		if(debug) debug_printf("关闭 P%c%d", _PIN_NAME(pin));
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
	if(Debug)
	{
		debug_printf(" %dM", Speed);
		if(OpenDrain)
			debug_printf(" 开漏");
		else
			debug_printf(" 推挽");
		if(Invert) debug_printf(" 倒置");
	}
#endif

	// 配置之前，需要根据倒置情况来设定初始状态，也就是在打开端口之前必须明确端口高低状态
	ushort dat = GPIO_ReadOutputData(Group);
	bool v = InitValue ^ Invert;
	if(!v)
		dat &= ~Mask;
	else
		dat |= Mask;
	GPIO_Write(Group, dat);

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

OutputPort& OutputPort::Init(Pin pin, bool invert)
{
	Port::Set(pin);

	Invert	= invert;

	return *this;
}
#endif

/******************************** OutputPort ********************************/

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
	bool rs = GPIO_ReadOutputData(Group) & Mask;
	return rs ^ Invert;
}

bool OutputPort::ReadInput()
{
	if(Empty()) return false;

	bool rs = GPIO_ReadInputData(Group) & Mask;
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
        GPIO_SetBits(Group, Mask);
    else
        GPIO_ResetBits(Group, Mask);
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

/******************************** InputPort ********************************/

// 输入端口
#define REGION_Input 1
#ifdef REGION_Input
/* 中断状态结构体 */
/* 一共16条中断线，意味着同一条线每一组只能有一个引脚使用中断 */
typedef struct TIntState
{
    InputPort*	Port;
    bool	OldValue;
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

void InputPort::Init(bool floating, PuPd_TypeDef pupd)
{
	PuPd		= pupd;
	Floating	= floating;

	Mode		= 0x03;
	//ShakeTime = 20;
	// 有些应用的输入口需要极高的灵敏度，这个时候不需要抖动检测
	ShakeTime	= 0;
	Invert		= false;
	HardEvent	= false;
	_taskInput	= 0;

	Handler		= NULL;
	Param		= NULL;
	_Value		= 0;
}

InputPort::~InputPort()
{
	Sys.RemoveTask(_taskInput);
}

InputPort& InputPort::Init(Pin pin, bool invert)
{
	Port::Set(pin);

	Invert	= invert;

	return *this;
}

ushort InputPort::ReadGroup() const    // 整组读取
{
	return GPIO_ReadInputData(Group);
}

// 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
bool InputPort::Read() const
{
	// 转为bool时会转为0/1
	bool rs = GPIO_ReadInputData(Group) & Mask;
	return rs ^ Invert;
}

bool InputPort::Read(Pin pin)
{
	GPIO_TypeDef* group = _GROUP(pin);
	return (group->IDR >> (pin & 0xF)) & 1;
}

void InputPort::OnPress(bool down)
{
	if(down)
	{
		if((Mode & 0x01) == 0) return;
	}
	else
	{
		if((Mode & 0x02) == 0) return;
	}

	if(HardEvent)
	{
		if(Handler) Handler(_Pin, down, Param);
	}
	else
	{
		// 允许两个值并存
		_Value	|= down ? 0x01 : 0x02;
		Sys.SetTask(_taskInput, true, ShakeTime);
	}
}

void InputPort::InputTask(void* param)
{
	InputPort* port = (InputPort*)param;
	if(port->Handler)
	{
		byte v = port->_Value;
		if(v)
		{
			port->_Value = 0;
			v &= port->Mode;
			if(v & 0x01) port->Handler(port->_Pin, true, port->Param);
			if(v & 0x02) port->Handler(port->_Pin, false, port->Param);
		}
	}
}

#define IT 1
#ifdef IT
void GPIO_ISR (int num)  // 0 <= num <= 15
{
	if(!hasInitState) return;

	IntState* st = States + num;
	if(!st) return;

	uint bit = 1 << num;
	bool value;
	//byte line = EXTI_Line0 << num;
	// 如果未指定委托，则不处理
	if(!st->Port) return;

	do {
		EXTI->PR = bit;   // 重置挂起位
		value = st->Port->Read(); // 获取引脚状态
	} while (EXTI->PR & bit); // 如果再次挂起则重复
	//EXTI_ClearITPendingBit(line);
	// 值必须有变动才触发
	//if(shakeTime > 0 && value == st->OldValue) return;
	st->OldValue = value;
	st->Port->OnPress(!value);
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
    ext.EXTI_Line		= EXTI_Line0 << pinIndex;
    ext.EXTI_Mode		= EXTI_Mode_Interrupt;
    ext.EXTI_Trigger	= EXTI_Trigger_Rising_Falling; // 上升沿下降沿触发
    ext.EXTI_LineCmd	= enable ? ENABLE : DISABLE;
    EXTI_Init(&ext);
}

void InputPort::OnOpen(GPIO_InitTypeDef& gpio)
{
	// 如果不是硬件事件，则默认使用20ms抖动
	if(!HardEvent) ShakeTime = 20;
#if DEBUG
	if(Debug)
	{
		debug_printf(" 抖动=%dus", ShakeTime);
		if(Floating)
			debug_printf(" 浮空");
		else if(PuPd == PuPd_UP)
			debug_printf(" 上拉");
		else if(PuPd == PuPd_DOWN)
			debug_printf(" 下拉");
		if(Invert) debug_printf(" 倒置");
	}
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

void InputPort::OnClose()
{
	Port::OnClose();

	int idx = Bits2Index(Mask);

    IntState* st = &States[idx];
	if(st->Port == this)
	{
		st->Port = NULL;

		SetEXIT(idx, false);

		Interrupt.Deactivate(PORT_IRQns[idx]);
	}
}

// 注册回调  及中断使能
void InputPort::Register(IOReadHandler handler, void* param)
{
    //if(!Mask) return;
	assert_param2(_Pin != P0, "输入注册必须先设置引脚");

    Handler	= handler;
	Param	= param;

    // 检查并初始化中断线数组
    if(!hasInitState)
    {
        for(int i=0; i<16; i++)
        {
            IntState* st = &States[i];
            st->Port	= NULL;
        }
        hasInitState = true;
    }

	byte gi = _Pin >> 4;
	int idx = Bits2Index(Mask);
	IntState* st = &States[idx];

	InputPort* port	= st->Port;
    // 检查是否已经注册到别的引脚上
    if(port != this && port != NULL)
    {
#if DEBUG
        debug_printf("中断线EXTI%d 不能注册到 P%c%d, 它已经注册到 P%c%d\r\n", gi, _PIN_NAME(_Pin), _PIN_NAME(port->_Pin));
#endif
        return;
    }
    st->Port		= this;
	st->OldValue	= Read(); // 预先保存当前状态值，后面跳变时触发中断

    // 打开时钟，选择端口作为端口EXTI时钟线
#if defined(STM32F0) || defined(STM32F4)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(gi, idx);
#elif defined(STM32F1)
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_EXTILineConfig(gi, idx);
#endif

	SetEXIT(idx, true);

    // 打开并设置EXTI中断为低优先级
    Interrupt.SetPriority(PORT_IRQns[idx], 1);

    Interrupt.Activate(PORT_IRQns[idx], EXTI_IRQHandler, this);

	if(!_taskInput && !HardEvent) _taskInput = Sys.AddTask(InputTask, this, -1, -1, "输入中断");
}

#endif
