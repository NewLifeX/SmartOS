#include "Port.h"

#ifdef STM32F10X
    #include "stm32f10x_exti.h"
#else
    #include "stm32f0xx_exti.h"
#endif

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

/*默认按键去抖延时   70ms*/
//static byte shake_time = 70;

// 16条中断线
static IntState State[16];
static bool hasInitState = false;

#ifdef STM32F10X
static int PORT_IRQns[] = {
    EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn, EXTI4_IRQn, // 5个基础的
    EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn,    // EXTI9_5
    EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn   // EXTI15_10
};
#else
static int PORT_IRQns[] = {
    EXTI0_1_IRQn, EXTI0_1_IRQn, // 基础
    EXTI2_3_IRQn, EXTI2_3_IRQn, // 基础
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, 
    EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn, EXTI4_15_IRQn   // EXTI15_10
};
#endif

void RegisterInput(int groupIndex, int pinIndex, InputPort::IOReadHandler handler);
void UnRegisterInput(int pinIndex);

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port
Port::Port()
{
	Group = 0;
	PinBit = 0;
	//Restore = false;

	// 特别要慎重，有些结构体成员可能因为没有初始化而酿成大错
	GPIO_StructInit(&gpio);
}

Port::~Port()
{
#ifdef STM32F10X
	// 恢复为初始化状态
	ushort bits = PinBit;
	int config = InitState & 0xFFFFFFFF;
	for(int i=0; i<16 && bits; i++, bits>>=1)
	{
		if(i == 7) config = InitState >> 32;
		if(bits & 1)
		{
			uint shift = (i & 7) << 2; // 每引脚4位
			uint mask = 0xF << shift;  // 屏蔽掉其它位

			if (i & 0x08) { // bit 8 - 15
				Group->CRH = Group->CRH & ~mask | (config & mask);
			} else { // bit 0-7
				Group->CRL = Group->CRL & ~mask | (config & mask);
			}
		}
	}
#endif

#if DEBUG
	// 解除保护引脚
	byte groupIndex = GroupToIndex(Group) << 4;
	ushort bits2 = PinBit;
	for(int i=0; i<16 && bits2; i++, bits2>>=1)
    {
        if(bits2 & 1) OnReserve((Pin)(groupIndex | i), false);
    }
#endif
}

void Port::OnSetPort()
{
#ifdef STM32F10X
	// 整组引脚的初始状态，析构时有选择恢复
	InitState = ((ulong)Group->CRH << 32) + Group->CRL;
#endif

#if DEBUG
	// 保护引脚
	byte groupIndex = GroupToIndex(Group) << 4;
	ushort bits = PinBit;
	for(int i=0; i<16 && bits; i++, bits>>=1)
    {
        if(bits & 1) OnReserve((Pin)(groupIndex | i), true);
    }
#endif
}

// 单一引脚初始化
void Port::SetPort(Pin pin)
{
    Group = IndexToGroup(pin >> 4);
    PinBit = IndexToBits(pin & 0x0F);
    Pin0 = pin;

	OnSetPort();
}

void Port::SetPort(GPIO_TypeDef* group, ushort pinbit)
{
    Group = group;
    PinBit = pinbit;
    
    Pin0 = (Pin)((GroupToIndex(group) << 4) + BitsToIndex(pinbit));

	OnSetPort();
}

// 用一组引脚来初始化，引脚组由第一个引脚决定，请确保所有引脚位于同一组
void Port::SetPort(Pin pins[], uint count)
{
	assert_param(pins != NULL && count > 0 && count <= 16);

    Group = IndexToGroup(pins[0] >> 4);
    PinBit = 0;
    for(int i=0; i<count; i++)
        PinBit |= IndexToBits(pins[i] & 0x0F);
    Pin0 = pins[0];

	OnSetPort();
}

void Port::Config()
{
    OnConfig();
    GPIO_Init(Group, &gpio);
}

void Port::OnConfig()
{
    // 打开时钟
    int gi = Port::GroupToIndex(Group);
#ifdef STM32F0XX
    RCC_AHBPeriphClockCmd(RCC_AHBENR_GPIOAEN << gi, ENABLE);
#else
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA << gi, ENABLE);
#endif

    gpio.GPIO_Pin = PinBit;
}

GPIO_TypeDef* Port::IndexToGroup(byte index) { return ((GPIO_TypeDef *) (GPIOA_BASE + (index << 10))); }
byte Port::GroupToIndex(GPIO_TypeDef* group) { return (byte)(((int)group - GPIOA_BASE) >> 10); }
ushort Port::IndexToBits(byte index) { return 1 << (ushort)(index & 0x0F); }
byte Port::BitsToIndex(ushort bits)
{
    for(int i=0; i < 16 & bits; i++)
    {
        if((bits & 1) == 1) return i;
        bits >>= 1;
    }
    return 0xFF;
}
#endif

// 端口引脚保护
#if DEBUG
static ushort Reserved[8];		// 引脚保留位，记录每个引脚是否已经被保留，禁止别的模块使用

// 保护引脚，别的功能要使用时将会报错。返回是否保护成功
bool Port::Reserve(Pin pin, bool flag)
{
    int port = pin >> 4, bit = 1 << (pin & 0x0F);
    if (flag) {
        if (Reserved[port] & bit) {
			// 增加针脚已经被保护的提示，很多地方调用ReservePin而不写日志，得到False后直接抛异常
			debug_printf("ReservePin P%c%d already reserved\r\n", _PIN_NAME(pin));
			return false; // already reserved
		}
        Reserved[port] |= bit;

		debug_printf("ReservePin P%c%d\r\n", _PIN_NAME(pin));
    } else {
        Reserved[port] &= ~bit;

#ifdef STM32F10X
		int config = 0;
		uint shift = (pin & 7) << 2; // 4 bits / pin
		uint mask = 0xF << shift; // 屏蔽掉其它位
		GPIO_TypeDef* port2 = IndexToGroup(pin >> 4); // pointer to the actual port registers 
		if (pin & 0x08) { // bit 8 - 15
			config = port2->CRH & mask;
		} else { // bit 0-7
			config = port2->CRL & mask;
		}

		config >>= shift;	// 移位到最右边
		config &= 0xF;
		debug_printf("UnReservePin P%c%d Config=0x%02x\r\n", _PIN_NAME(pin), config);
#else
		debug_printf("UnReservePin P%c%d\r\n", _PIN_NAME(pin));
#endif
	}

    return true;
}

bool Port::OnReserve(Pin pin, bool flag)
{
	//OnReserve(pin, flag);
	return Reserve(pin, flag);
}

bool OutputPort::OnReserve(Pin pin, bool flag)
{
	debug_printf("Output ");

	return Port::OnReserve(pin, flag);
}

bool AlternatePort::OnReserve(Pin pin, bool flag)
{
	debug_printf("Alternate ");

	return Port::OnReserve(pin, flag);
}

bool InputPort::OnReserve(Pin pin, bool flag)
{
	debug_printf("Input ");

	return Port::OnReserve(pin, flag);
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
void InputOutputPort::OnConfig()
{
	assert_param(Speed == 2 || Speed == 10 || Speed == 50);

	Port::OnConfig();

	switch(Speed)
	{
		case 2: gpio.GPIO_Speed = GPIO_Speed_2MHz; break;
		case 10: gpio.GPIO_Speed = GPIO_Speed_10MHz; break;
		case 50: gpio.GPIO_Speed = GPIO_Speed_50MHz; break;
	}
}

void OutputPort::OnConfig()
{
	InputOutputPort::OnConfig();

#ifdef STM32F0XX
	gpio.GPIO_Mode = GPIO_Mode_OUT;
	gpio.GPIO_OType = OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;
#else
	gpio.GPIO_Mode = OpenDrain ? GPIO_Mode_Out_OD : GPIO_Mode_Out_PP;
#endif
}

void AlternatePort::OnConfig()
{
	InputOutputPort::OnConfig();

#ifdef STM32F0XX
	gpio.GPIO_Mode = GPIO_Mode_AF;
	gpio.GPIO_OType = OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;
#else
	gpio.GPIO_Mode = OpenDrain ? GPIO_Mode_AF_OD : GPIO_Mode_AF_PP;
#endif
}

void InputPort::OnConfig()
{
	InputOutputPort::OnConfig();

#ifdef STM32F0XX
	gpio.GPIO_Mode = GPIO_Mode_IN;
	//gpio.GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;
#else
	if(Floating)
		gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	else if(PuPd == PuPd_UP)
		gpio.GPIO_Mode = GPIO_Mode_IPU;
	else if(PuPd == PuPd_DOWN)
		gpio.GPIO_Mode = GPIO_Mode_IPD; // 这里很不确定，需要根据实际进行调整
#endif
}

void AnalogPort::OnConfig()
{
	Port::OnConfig();

#ifdef STM32F0XX
	gpio.GPIO_Mode = GPIO_Mode_AN;
	//gpio.GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;
#else
	gpio.GPIO_Mode = GPIO_Mode_AIN; // 这里很不确定，需要根据实际进行调整
#endif
}
#endif

// 输出端口
#define REGION_Output 1
#ifdef REGION_Output
// 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
bool InputOutputPort::Read()
{
    return (ReadGroup() & PinBit) ^ Invert;
}

// 读取端口状态
bool InputOutputPort::Read(Pin pin)
{
    GPIO_TypeDef* group = _GROUP(pin);
    return (group->IDR >> (pin & 0xF)) & 1;
}

void OutputPort::Write(bool value)
{
    if(value ^ Invert)
        GPIO_SetBits(Group, PinBit);
    else
        GPIO_ResetBits(Group, PinBit);
}

void OutputPort::WriteGroup(ushort value)
{
    GPIO_Write(Group, value);
}

void OutputPort::Up(uint ms)
{
    Write(true);
	Sys.Sleep(ms);
    Write(false);
}

void OutputPort::Blink(uint times, uint ms)
{
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
    if(value)
        GPIO_SetBits(_GROUP(pin), _PORT(pin));
    else
        GPIO_ResetBits(_GROUP(pin), _PORT(pin));
}
#endif

// 输入端口
#define REGION_Output 1
#ifdef REGION_Output
InputPort::~InputPort()
{
    // 取消所有中断
    if(_Registed) Register(NULL);
}

// 注册回调  及中断使能
void InputPort::Register(IOReadHandler handler, void* param)
{
    if(!PinBit) return;

    // 检查并初始化中断线数组
    if(!hasInitState)
    {
        //State = new IntState[16];
        for(int i=0; i<16; i++)
        {
            IntState* state = &State[i];
            state->Pin = P0;
            state->Handler = NULL;
            state->Used = 0;
        }
        hasInitState = true;
    }

    byte groupIndex = GroupToIndex(Group);
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
                RegisterInput(groupIndex, i, handler, param);
            }
            else
                UnRegisterInput(i);
        }
        n >>= 1;
    }
    
    _Registed = handler != NULL;
}


//.............................中断函数处理部分.............................
#define IT 1
#ifdef IT
extern "C"
{
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
        if(shakeTime == 0) shakeTime = 20;

        do {
            //value = TIO_Read(state->Pin); // 获取引脚状态
            EXTI->PR = bit;   // 重置挂起位
            value = InputOutputPort::Read(state->Pin); // 获取引脚状态
			// 值必须有变动才触发
			if(value == state->OldValue) return;

			Time.Sleep(shakeTime); // 避免抖动
        } while (EXTI->PR & bit); // 如果再次挂起则重复
        //EXTI_ClearITPendingBit(line);
		// 值必须有变动才触发
		if(value == state->OldValue) return;
		state->OldValue = value;
        if(state->Handler)
        {
            state->Handler(state->Pin, value, state->Param);
        }
    }

    void EXTI_IRQHandler(ushort num, void* param)
    {
#ifdef STM32F10X
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
#else
    //stm32f0xx
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

    // 打开时钟，选择端口作为端口EXTI时钟线
#ifdef STM32F0XX
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
    SYSCFG_EXTILineConfig(groupIndex, pinIndex);
#else
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_EXTILineConfig(groupIndex, pinIndex);
#endif
    /* 配置EXTI中断线 */
    EXTI_InitTypeDef ext;
    EXTI_StructInit(&ext);
    ext.EXTI_Line = EXTI_Line0 << pinIndex;
    ext.EXTI_Mode = EXTI_Mode_Interrupt;
    ext.EXTI_Trigger = EXTI_Trigger_Rising_Falling; // 上升沿下降沿触发
    ext.EXTI_LineCmd = ENABLE;
    EXTI_Init(&ext);

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

    EXTI_InitTypeDef ext;
    ext.EXTI_Line = EXTI_Line0 << pinIndex;
    ext.EXTI_Mode = EXTI_Mode_Interrupt;
    ext.EXTI_Trigger = EXTI_Trigger_Rising_Falling; // 上升沿下降沿触发
    ext.EXTI_LineCmd = DISABLE;
    EXTI_Init(&ext);

    state->Used--;
    if(state->Used == 0)
    {
        Interrupt.Deactivate(PORT_IRQns[pinIndex]);
    }
}
#endif

