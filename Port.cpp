#include "Port.h"

#if defined(STM32F1) || defined(STM32F4)
static const int PORT_IRQns[] = {
    EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn, EXTI4_IRQn, // 5个基础的
    EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn,    // EXTI9_5
    EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn   // EXTI15_10
};
#elif defined(STM32F0) || defined(GD32F150)
static const int PORT_IRQns[] = {
    EXTI0_1_IRQn, EXTI0_1_IRQn, // 基础
    EXTI2_3_IRQn, EXTI2_3_IRQn, // 基础
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn,
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn   // EXTI15_10
};
#endif

_force_inline GPIO_TypeDef* IndexToGroup(byte index) { return ((GPIO_TypeDef *) (GPIOA_BASE + (index << 10))); }
_force_inline byte GroupToIndex(GPIO_TypeDef* group) { return (byte)(((int)group - GPIOA_BASE) >> 10); }

/******************************** Port ********************************/

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port
/* Port::Port()
{
	_Pin	= P0;
	Group	= NULL;
	Mask	= 0;
	Opened	= false;
} */

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

// 分组时钟
static byte _GroupClock[10];

void OpenClock(Pin pin, bool flag)
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
bool Port::Open()
{
	if(_Pin == P0) return false;
	if(Opened) return true;

	TS("Port::Open");

    // 先打开时钟才能配置
	OpenClock(_Pin, true);

#if DEBUG
	// 保护引脚
	//Show();
	GetType().Name().Show();
	Reserve(_Pin, true);
#endif

	GPIO_InitTypeDef gpio;
	// 特别要慎重，有些结构体成员可能因为没有初始化而酿成大错
	GPIO_StructInit(&gpio);

    OnOpen(gpio);
    GPIO_Init(Group, &gpio);

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
	//Show();
	GetType().Name().Show();
	Reserve(_Pin, false);
	debug_printf("\r\n");
#endif

	Opened = false;
}

void Port::OnClose()
{
	// 不能随便关闭时钟，否则可能会影响别的引脚
	OpenClock(_Pin, false);
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

#if defined(STM32F0) || defined(GD32F150) || defined(STM32F4)
void Port::AFConfig(byte GPIO_AF) const
{
	assert_param2(Opened, "必须打开端口以后才能配置AF");

	GPIO_PinAFConfig(Group, _PIN(_Pin), GPIO_AF);
}
#endif

bool Port::Read() const
{
	if(_Pin == P0) return false;

	return GPIO_ReadInputData(Group) & Mask;
}
#endif

// 端口引脚保护
#if DEBUG
// 引脚保留位，记录每个引脚是否已经被保留，禁止别的模块使用
// !!!注意，不能全零，否则可能会被当作zeroinit而不是copy，导致得不到初始化
static ushort Reserved[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF};

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

/******************************** OutputPort ********************************/

// 输出端口
#define REGION_Output 1
#ifdef REGION_Output

OutputPort::OutputPort() : Port() { }
OutputPort::OutputPort(Pin pin) : OutputPort(pin, 2) { }
OutputPort::OutputPort(Pin pin, byte invert, bool openDrain, byte speed) : Port()
{
	OpenDrain	= openDrain;
	Speed		= speed;
	Invert		= invert;

	if(pin != P0)
	{
		Set(pin);
		Open();
	}
}

OutputPort& OutputPort::Init(Pin pin, bool invert)
{
	Port::Set(pin);

	Invert	= invert;

	return *this;
}

void OutputPort::OnOpen(GPIO_InitTypeDef& gpio)
{
	TS("OutputPort::OnOpen");

#ifndef STM32F4
	assert_param(Speed == 2 || Speed == 10 || Speed == 50);
#else
	assert_param(Speed == 2 || Speed == 25 || Speed == 50 || Speed == 100);
#endif

#if DEBUG
	debug_printf(" %dM", Speed);
	if(OpenDrain)
		debug_printf(" 开漏");
	else
		debug_printf(" 推挽");
	bool fg	= false;
#endif

	// 根据倒置情况来获取初始状态，自动判断是否倒置
	bool rs = Port::Read();
	if(Invert > 1)
	{
		Invert	= rs;
#if DEBUG
		fg		= true;
#endif
	}

#if DEBUG
	if(Invert)
	{
		if(fg)
			debug_printf(" 自动倒置");
		else
			debug_printf(" 倒置");
	}
#endif

#if DEBUG
	debug_printf(" 初始电平=%d \r\n", rs);
#endif

	Port::OnOpen(gpio);

	switch(Speed)
	{
		case 2:		gpio.GPIO_Speed = GPIO_Speed_2MHz;	break;
#ifndef STM32F4
		case 10:	gpio.GPIO_Speed = GPIO_Speed_10MHz;	break;
#else
		case 25:	gpio.GPIO_Speed = GPIO_Speed_25MHz;	break;
		case 100:	gpio.GPIO_Speed = GPIO_Speed_100MHz;break;
#endif
		case 50:	gpio.GPIO_Speed = GPIO_Speed_50MHz;	break;
	}

#ifdef STM32F1
	gpio.GPIO_Mode	= OpenDrain ? GPIO_Mode_Out_OD : GPIO_Mode_Out_PP;
#else
	gpio.GPIO_Mode	= GPIO_Mode_OUT;
	gpio.GPIO_OType	= OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;
#endif
}

bool OutputPort::Read() const
{
	if(Empty()) return false;

	// 转为bool时会转为0/1
	bool rs = GPIO_ReadOutputData(Group) & Mask;
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
        GPIO_SetBits(Group, Mask);
    else
        GPIO_ResetBits(Group, Mask);
}

void OutputPort::Up(uint ms) const
{
	if(Empty()) return;

    Write(true);
	Sys.Sleep(ms);
    Write(false);
}

void OutputPort::Down(uint ms) const
{
	if(Empty()) return;

    Write(false);
	Sys.Sleep(ms);
    Write(true);
}

void OutputPort::Blink(uint times, uint ms) const
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

/******************************** AlternatePort ********************************/

AlternatePort::AlternatePort() : OutputPort(P0, false, false) { }
AlternatePort::AlternatePort(Pin pin) : OutputPort(pin, false, false) { }
AlternatePort::AlternatePort(Pin pin, byte invert, bool openDrain, byte speed)
	: OutputPort(pin, invert, openDrain, speed) { }

void AlternatePort::OnOpen(GPIO_InitTypeDef& gpio)
{
	OutputPort::OnOpen(gpio);

#ifdef STM32F1
	gpio.GPIO_Mode	= OpenDrain ? GPIO_Mode_AF_OD : GPIO_Mode_AF_PP;
#else
	gpio.GPIO_Mode	= GPIO_Mode_AF;
	gpio.GPIO_OType	= OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;
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

InputPort::InputPort() : InputPort(P0) { }
InputPort::InputPort(Pin pin, bool floating, PuPd pull) : Port()
{
	/* Pull		= pull;
	Floating	= floating;

	Invert		= 2;

	Mode		= Both;
	ShakeTime	= 0;
	HardEvent	= false; */
	_taskInput	= 0;

	Handler		= NULL;
	Param		= NULL;
	_Value		= 0;

	_PressStart = 0;
	_PressStart2 = 0;
	PressTime	= 0;
	_PressLast	= 0;

	if(pin != P0)
	{
		Set(pin);
		Open();
	}
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

// 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
bool InputPort::Read() const
{
	return Port::Read() ^ Invert;
}

void InputPort::OnPress(bool down)
{
	//debug_printf("OnPress P%c%d down=%d Invert=%d 时间=%d\r\n", _PIN_NAME(_Pin), down, Invert, PressTime);
	/*
	！！！注意：
	有些按钮可能会出现110现象，也就是按下的时候1（正常），弹起的时候连续的1和0（不正常）。
	解决思路：
	1，预处理抖动，先把上一次干掉（可能已经被处理）
	2，记录最近两次按下的时间，如果出现抖动且时间相差不是非常大，则使用上一次按下时间
	3，也可能出现100抖动
	*/
	ulong	now	= Sys.Ms();
	// 预处理抖动。如果相邻两次间隔小于抖动时间，那么忽略上一次未处理的值（可能失败）
	bool shake	= false;
	if(ShakeTime && ShakeTime > now - _PressLast)
	{
		_Value	= 0;
		shake	= true;
	}

	if(down)
	{
		_PressStart2	= _PressStart;
		_PressStart		= now;
	}
	else
	{
		// 如果这次是弹起，倒退按下的时间。为了避免较大的误差，限定10秒内
		if(shake && _PressStart2 > 0 && _PressStart2 + 10000 >= _PressStart)
		{
			_PressStart	= _PressStart2;
			_PressStart2	= 0;
		}

		if (_PressStart > 0) PressTime	= now - _PressStart;
	}
	_PressLast	= now;

	if(down)
	{
		if((Mode & Rising) == 0) return;
	}
	else
	{
		if((Mode & Falling) == 0) return;
	}

	if(HardEvent)
	{
		if(Handler) Handler(this, down, Param);
	}
	else
	{
		// 允许两个值并存
		//_Value	|= down ? Rising : Falling;
		_Value	= down ? Rising : Falling;
		Sys.SetTask(_taskInput, true, ShakeTime);
	}
}

void InputPort::InputTask(void* param)
{
	auto port = (InputPort*)param;
	byte v = port->_Value;
	if(!v) return;

	if(port->Handler)
	{
		port->_Value = 0;
		v &= port->Mode;
		if(v & Rising)	port->Handler(port, true, port->Param);
		if(v & Falling)	port->Handler(port, false, port->Param);
	}
}

#define IT 1
#ifdef IT
void GPIO_ISR(int num)  // 0 <= num <= 15
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
	//st->Port->OnPress(value ^ st->Port->Invert);
	// Read的时候已经计算倒置，这里不必重复计算
	st->Port->OnPress(value);
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
#elif defined(STM32F0) || defined(GD32F150)
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

void InputPort::OnOpen(GPIO_InitTypeDef& gpio)
{
	TS("InputPort::OnOpen");

	// 如果不是硬件事件，则默认使用20ms抖动
	if(!HardEvent) ShakeTime = 20;
#if DEBUG
	debug_printf(" 抖动=%dms", ShakeTime);
	if(Floating)
		debug_printf(" 浮空");
	else if(Pull == UP)
		debug_printf(" 上升沿");
	else if(Pull == DOWN)
		debug_printf(" 下降沿");
	if(Mode & Rising)	debug_printf(" 按下");
	if(Mode & Falling)	debug_printf(" 弹起");

	bool fg	= false;
#endif

	// 根据倒置情况来获取初始状态，自动判断是否倒置
	bool rs = Port::Read();
	if(Invert > 1)
	{
		Invert	= rs;
#if DEBUG
		fg		= true;
#endif
	}

#if DEBUG
	if(Invert)
	{
		if(fg)
			debug_printf(" 自动倒置");
		else
			debug_printf(" 倒置");
	}
#endif

#if DEBUG
	debug_printf(" 初始电平=%d \r\n", rs);
#endif

	Port::OnOpen(gpio);

#ifdef STM32F1
	if(Floating)
		gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	else if(Pull == UP)
		gpio.GPIO_Mode = GPIO_Mode_IPU;
	else if(Pull == DOWN)
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

		SetEXIT(idx, false, GetTrigger(Mode, Invert));

		Interrupt.Deactivate(PORT_IRQns[idx]);
	}
}

// 注册回调  及中断使能
bool InputPort::Register(IOReadHandler handler, void* param)
{
	assert_param2(_Pin != P0, "输入注册必须先设置引脚");

    Handler	= handler;
	Param	= param;

    // 检查并初始化中断线数组
    if(!hasInitState)
    {
        for(int i=0; i<16; i++)
        {
            auto st = &States[i];
            st->Port	= NULL;
        }
        hasInitState = true;
    }

	byte gi = _Pin >> 4;
	int idx = Bits2Index(Mask);
	auto st = &States[idx];

	auto port	= st->Port;
    // 检查是否已经注册到别的引脚上
    if(port != this && port != NULL)
    {
#if DEBUG
        debug_printf("中断线EXTI%d 不能注册到 P%c%d, 它已经注册到 P%c%d\r\n", gi, _PIN_NAME(_Pin), _PIN_NAME(port->_Pin));
#endif

		// 将来可能修改设计，即使注册失败，也可以开启一个短时间的定时任务，来替代中断输入
        return false;
    }
    st->Port		= this;
	st->OldValue	= Read(); // 预先保存当前状态值，后面跳变时触发中断

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

	if(!_taskInput && !HardEvent) _taskInput = Sys.AddTask(InputTask, this, -1, -1, "输入中断");

	return true;
}

#endif

/******************************** AnalogInPort ********************************/

void AnalogInPort::OnOpen(GPIO_InitTypeDef& gpio)
{
#if DEBUG
	debug_printf("\r\n");
#endif

	Port::OnOpen(gpio);

#ifdef STM32F1
	gpio.GPIO_Mode	= GPIO_Mode_AIN; //
#else
	gpio.GPIO_Mode	= GPIO_Mode_AN;
	//gpio.GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;
#endif
}
