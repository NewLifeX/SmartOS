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
    InputPort::IOReadHandler Handler;
    bool OldValue;
} IntState;

/*默认按键去抖延时   70ms*/
static byte shake_time = 70;

// 16条中断线
static IntState* State;

// 单一引脚初始化
void Port::SetPort(Pin pin)
{
    Group = IndexToGroup(pin >> 4);
    PinBit = IndexToBits(pin & 0x0F);
}

void Port::SetPort(GPIO_TypeDef* group, ushort pinbit)
{
    Group = group;
    PinBit = pinbit;
}

// 用一组引脚来初始化，引脚组由第一个引脚决定，请确保所有引脚位于同一组
void Port::SetPort(Pin pins[])
{
    Group = IndexToGroup(pins[0] >> 4);
    PinBit = 0;
    for(int i=0; i<sizeof(pins)/sizeof(Pin); i++)
        PinBit |= IndexToBits(pins[i] & 0x0F);
}

// mode=Mode_IN/Mode_OUT/Mode_AF/Mode_AN
// speed=Speed_50MHz/Speed_2MHz/Speed_10MHz
// type=OType_PP/OType_OD
// PuPd= PuPd_NOPULL/ PuPd_UP  /PuPd_DOWN
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
    //gpio.GPIO_Speed = (GPIOSpeed_TypeDef)speed;
#ifdef STM32F0XX
    gpio.GPIO_Mode = mode;
    gpio.GPIO_OType = isOD ? GPIO_OType_OD : GPIO_OType_PP;	
#else
    switch(Mode)
    {
        case Port::Mode_IN:
            //p->GPIO_Mode = isOD ? GPIO_Mode_AIN : GPIO_Mode_IN_FLOATING;
            break;
        case Port::Mode_OUT:
            //p->GPIO_Mode = isOD ? GPIO_Mode_Out_OD : GPIO_Mode_Out_PP;
            break;
        case Port::Mode_AF:
            //p->GPIO_Mode = isOD ? GPIO_Mode_AF_OD : GPIO_Mode_AF_PP;
            break;
        case Port::Mode_AN:
            //p->GPIO_Mode = isOD ? GPIO_Mode_IPD : GPIO_Mode_IPU;
            break;
    }
#endif
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

// 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
bool InputOutputPort::Read()
{
    // 最低那一个位的索引
    //byte idx = BitsToIndex(PinBit);
    return (ReadGroup() & PinBit) ^ Invert;
}

ushort InputOutputPort::ReadGroup()
{
    switch(Mode)
    {
        case Mode_IN:
        case Mode_AN:
            return GPIO_ReadInputData(Group);
        case Mode_OUT:
        case Mode_AF:
            return GPIO_ReadOutputData(Group);
        default:
            return 0;
    }
}

// 打开端口
/*void Port::Set(Pin pin, Port::Mode_TypeDef mode, bool isOD, Port::Speed_TypeDef speed, Port::PuPd_TypeDef pupd)
{
    SetPort(_GROUP(pin), _PORT(pin), mode, isOD, speed, pupd);
}

void Port::SetInput(Pin pin, bool isFloating, Port::Speed_TypeDef speed)
{
    Set(pin, Port::Mode_IN, !isFloating, speed);
}

void Port::SetOutput(Pin pin, bool isOD, Port::Speed_TypeDef speed)
{
    Set(pin, Port::Mode_OUT, isOD, speed);
}

void Port::SetAlternate(Pin pin, bool isOD, Port::Speed_TypeDef speed)
{
    Set(pin, Port::Mode_AF, isOD, speed);
}

void Port::SetAnalog(Pin pin)
{
    Set(pin, Port::Mode_AN);
}*/

// 设置端口状态
void OutputPort::Write(Pin pin, bool value)
{
    if(value)
        GPIO_SetBits(_GROUP(pin), _PORT(pin));
    else
        GPIO_ResetBits(_GROUP(pin), _PORT(pin));
}

// 读取端口状态
bool InputOutputPort::Read(Pin pin)
{
    GPIO_TypeDef* group = _GROUP(pin);
    return (group->IDR >> (pin & 0xF)) & 1;
}

// 注册回调  及中断使能
void InputPort::Register(IOReadHandler handler)
{
    // 检查并初始化中断线数组
    if(!State)
    {
        State = new IntState[16];
        for(int i=0; i<16; i++)
        {
            State[i].Pin = P0;
            State[i].Handler = NULL;
        }
    }

    byte pins = GroupToIndex(Group);
    IntState* state = &State[pins];
    // 注册中断事件
    if(handler)
    {
        ushort pin = (GroupToIndex(Group) << 4) + BitsToIndex(PinBit);
        // 检查是否已经注册到别的引脚上
        if(state->Pin != pin && state->Pin != P0)
        {
#if DEBUG
            printf("EXTI%d can't register to P%c%d, it has register to P%c%d\r\n", pins, _PIN_NAME(pin), _PIN_NAME(state->Pin));
#endif
            return;
        }
        state->Pin = pin;
        state->Handler = handler;
        /* 打开时钟 */
#ifdef STM32F0XX
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
        SYSCFG_EXTILineConfig(pin>>4, pins);
#else
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
        GPIO_EXTILineConfig(pin>>4, pins);
#endif
        /* 配置EXTI中断线 */
        EXTI_InitTypeDef ext;
        EXTI_StructInit(&ext);
        //ext.EXTI_Line = EXTI_Line0;
        ext.EXTI_Line = EXTI_Line0 << pins;
        ext.EXTI_Mode = EXTI_Mode_Interrupt;
        ext.EXTI_Trigger = EXTI_Trigger_Rising_Falling;  
        ext.EXTI_LineCmd = ENABLE;
        EXTI_Init(&ext);

        // 打开并设置EXTI中断为低优先级
        NVIC_InitTypeDef nvic;
#ifdef STM32F10X
        //nvic.NVIC_IRQChannel = EXTI0_IRQn;
        if(pins < 5)
            nvic.NVIC_IRQChannel = EXTI0_IRQn + 6;
        else if(pins < 10)
            nvic.NVIC_IRQChannel = EXTI9_5_IRQn;
        else
            nvic.NVIC_IRQChannel = EXTI15_10_IRQn;
        nvic.NVIC_IRQChannelPreemptionPriority = 0xff;
        nvic.NVIC_IRQChannelSubPriority = 0xff;
#else
        if(pins < 0x02)
            nvic.NVIC_IRQChannel = EXTI0_1_IRQn;
        if(pins < 0x04)
            nvic.NVIC_IRQChannel = EXTI2_3_IRQn;
        else
            nvic.NVIC_IRQChannel = EXTI4_15_IRQn;
        nvic.NVIC_IRQChannelPriority = 0x01;		//为滴答定时器让道  中断优先级不为最高
#endif
		nvic.NVIC_IRQChannelCmd = ENABLE;

        NVIC_Init(&nvic);
    }
    else
    {
        // 取消注册
        state->Pin = P0;
        state->Handler = 0;
    }
}

//.............................中断函数处理部分.............................
#define IT 1
#ifdef IT
extern "C"
{
    void GPIO_ISR (int num)  // 0 <= num <= 15
    {
        if(!State) return;

        IntState* state = State + num;
        if(!state) return;

        uint bit = 1 << num;
        bool value;
        //byte line = EXTI_Line0 << num;
        // 如果未指定委托，则不处理
        if(!state->Handler) return;
        //if(EXTI_GetITStatus(line) == RESET) return;
        do {
            //value = TIO_Read(state->Pin); // 获取引脚状态
            EXTI->PR = bit;   // 重置挂起位
            value = InputOutputPort::Read(state->Pin); // 获取引脚状态
            Sys.Sleep(shake_time); // 避免抖动		在os_cfg.h里面修改
        } while (EXTI->PR & bit); // 如果再次挂起则重复
        //EXTI_ClearITPendingBit(line);
        if(state->Handler)
        {
            state->Handler(state->Pin, value);
        }
    }

#ifdef STM32F10X
    void EXTI0_IRQHandler (void) { GPIO_ISR(0); } // EXTI0
    void EXTI1_IRQHandler (void) { GPIO_ISR(1); } // EXTI1
    void EXTI2_IRQHandler (void) { GPIO_ISR(2); } // EXTI2
    void EXTI3_IRQHandler (void) { GPIO_ISR(3); } // EXTI3
    void EXTI4_IRQHandler (void) { GPIO_ISR(4); } // EXTI4
    void EXTI9_5_IRQHandler (void) // EXTI5 - EXTI9
    {
        uint pending = EXTI->PR & EXTI->IMR & 0x03E0; // pending bits 5..9
        int num = 5; pending >>= 5;
        do {
            if (pending & 1) GPIO_ISR(num);
            num++; pending >>= 1;
        } while (pending);
    }

    void EXTI15_10_IRQHandler (void) // EXTI10 - EXTI15
    {
        uint pending = EXTI->PR & EXTI->IMR & 0xFC00; // pending bits 10..15
        int num = 10; pending >>= 10;
        do {
            if (pending & 1) GPIO_ISR(num);
            num++; pending >>= 1;
        } while (pending);
    }
#else			
    //stm32f0xx					
    void EXTI0_1_IRQHandler(void)
    {
        uint pending = EXTI->PR & EXTI->IMR & 0x0003; // pending bits 0..1
        int num = 0; pending >>= 0;
        do {
            if (pending & 1) GPIO_ISR(num);
            num++; pending >>= 1;
        } while (pending);
    }	

    void EXTI2_3_IRQHandler(void)
    {
        uint pending = EXTI->PR & EXTI->IMR & 0x000c; // pending bits 3..2
        int num = 2; pending >>= 2;
        do {
            if (pending & 1) GPIO_ISR(num);
            num++; pending >>= 1;
        } while (pending);
    }	

    void EXTI4_15_IRQHandler(void)
    {
        uint pending = EXTI->PR & EXTI->IMR & 0xFFF0; // pending bits 4..15
        int num = 4; pending >>= 4;
        do {
            if (pending & 1) GPIO_ISR(num);
            num++; pending >>= 1;
        } while (pending);
    }	
#endif		//STM32F10X

#endif 		//IT
}

void InputPort::SetShakeTime(byte ms)
{
	shake_time = ms;
}

GPIO_TypeDef* Port::IndexToGroup(byte index) { return ((GPIO_TypeDef *) (GPIOA_BASE + (index << 10))); }
byte Port::GroupToIndex(GPIO_TypeDef* group) { return (byte)(((int)group - GPIOA_BASE) >> 10); }
ushort Port::IndexToBits(byte index) { return 1 << (ushort)(index & 0x0F); }
byte Port::BitsToIndex(byte bits)
{
    for(int i=0; i < 16; i++)
    {
        if((bits & 1) == 1) return i;
    }
    return 0xFF;
}
