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

    uint ShakeTime;     // 抖动时间
    int Used;   // 被使用次数。对于前5行中断来说，这个只会是1，对于后面的中断线来说，可能多个
} IntState;

/*默认按键去抖延时   70ms*/
//static byte shake_time = 70;

// 16条中断线
static IntState State[16];
static bool hasInitState = false;

static int PORT_IRQns[] = { EXTI0_IRQn, EXTI1_IRQn, EXTI2_IRQn, EXTI3_IRQn, EXTI4_IRQn, // 5个基础的
                       EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn, EXTI9_5_IRQn,    // EXTI9_5
                       EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn, EXTI15_10_IRQn   // EXTI15_10
                        };

void RegisterInput(int groupIndex, int pinIndex, InputPort::IOReadHandler handler);
void UnRegisterInput(int pinIndex);

#define REGION_Port 1
#ifdef REGION_Port
// 单一引脚初始化
void Port::SetPort(Pin pin)
{
    Group = IndexToGroup(pin >> 4);
    PinBit = IndexToBits(pin & 0x0F);
    Pin0 = pin;
}

void Port::SetPort(GPIO_TypeDef* group, ushort pinbit)
{
    Group = group;
    PinBit = pinbit;
    
    Pin0 = (Pin)((GroupToIndex(group) << 4) + BitsToIndex(pinbit));
}

// 用一组引脚来初始化，引脚组由第一个引脚决定，请确保所有引脚位于同一组
void Port::SetPort(Pin pins[], uint count)
{
    Group = IndexToGroup(pins[0] >> 4);
    PinBit = 0;
    for(int i=0; i<count; i++)
        PinBit |= IndexToBits(pins[i] & 0x0F);
    Pin0 = pins[0];
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
    for(int i=0; i < 16; i++)
    {
        if((bits & 1) == 1) return i;
        bits >>= 1;
    }
    return 0xFF;
}
#endif

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

InputPort::~InputPort()
{
    // 取消所有中断
    if(_Registed) Register(NULL);
}

// 注册回调  及中断使能
void InputPort::Register(IOReadHandler handler)
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
                RegisterInput(groupIndex, i, handler);
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
        //if(EXTI_GetITStatus(line) == RESET) return;
        do {
            //value = TIO_Read(state->Pin); // 获取引脚状态
            EXTI->PR = bit;   // 重置挂起位
            value = InputOutputPort::Read(state->Pin); // 获取引脚状态
            Sys.Sleep(state->ShakeTime); // 避免抖动
        } while (EXTI->PR & bit); // 如果再次挂起则重复
        //EXTI_ClearITPendingBit(line);
        if(state->Handler)
        {
            state->Handler(state->Pin, value);
        }
    }

#ifdef STM32F10X
    void EXTI_IRQHandler(uint num, void* param)
    {
        // EXTI0 - EXTI4
        if(num < EXTI4_IRQn)
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
    }
#else
    //stm32f0xx
    void EXTI_IRQHandler(uint num, void* param)
    {
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
    }
#endif

#endif
}

void OutputPort::WriteGroup(ushort value)
{
    GPIO_Write(Group, value);
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

// 申请引脚中断托管
void InputPort::RegisterInput(int groupIndex, int pinIndex, IOReadHandler handler)
{
    IntState* state = &State[pinIndex];
    Pin pin = (Pin)((groupIndex << 4) + pinIndex);
    // 检查是否已经注册到别的引脚上
    if(state->Pin != pin && state->Pin != P0)
    {
#if DEBUG
        printf("EXTI%d can't register to P%c%d, it has register to P%c%d\r\n", groupIndex, _PIN_NAME(pin), _PIN_NAME(state->Pin));
#endif
        return;
    }
    state->Pin = pin;
    state->Handler = handler;

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
    //ext.EXTI_Line = EXTI_Line0;
    ext.EXTI_Line = EXTI_Line0 << pinIndex;
    ext.EXTI_Mode = EXTI_Mode_Interrupt;
    ext.EXTI_Trigger = EXTI_Trigger_Rising_Falling; // 上升沿下降沿触发
    ext.EXTI_LineCmd = ENABLE;
    EXTI_Init(&ext);

    // 打开并设置EXTI中断为低优先级
    NVIC_InitTypeDef nvic;
#ifdef STM32F10X
    //nvic.NVIC_IRQChannel = EXTI0_IRQn;
    if(groupIndex < 5)
        nvic.NVIC_IRQChannel = EXTI0_IRQn + groupIndex;
    else if(groupIndex < 10)
        nvic.NVIC_IRQChannel = EXTI9_5_IRQn;
    else
        nvic.NVIC_IRQChannel = EXTI15_10_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 0x01;
    nvic.NVIC_IRQChannelSubPriority = 0x01;
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

    state->Used++;
    if(state->Used == 1)
    {
        Interrupt.Activate(PORT_IRQns[pinIndex], EXTI_IRQHandler, this);
        /*switch(pinIndex)
        {
            case 0: Interrupt.Activate(EXTI0_IRQn, EXTI_IRQHandler, this); break;
            case 1: Interrupt.Activate(EXTI1_IRQn, EXTI_IRQHandler, this); break;
            case 2: Interrupt.Activate(EXTI2_IRQn, EXTI_IRQHandler, this); break;
            case 3: Interrupt.Activate(EXTI3_IRQn, EXTI_IRQHandler, this); break;
            case 4: Interrupt.Activate(EXTI4_IRQn, EXTI_IRQHandler, this); break;
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                Interrupt.Activate(EXTI9_5_IRQn, EXTI_IRQHandler, this); break;
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
                Interrupt.Activate(EXTI15_10_IRQn, EXTI_IRQHandler, this); break;
        }*/
    }
}

void InputPort::UnRegisterInput(int pinIndex)
{
    IntState* state = &State[pinIndex];
    // 取消注册
    state->Pin = P0;
    state->Handler = 0;

    EXTI_InitTypeDef ext;
    ext.EXTI_LineCmd = DISABLE;
    EXTI_Init(&ext);

    NVIC_InitTypeDef nvic;
    nvic.NVIC_IRQChannelCmd = DISABLE;

    NVIC_Init(&nvic);

    state->Used--;
    if(state->Used == 0)
    {
        Interrupt.Deactivate(PORT_IRQns[pinIndex]);
        /*switch(pinIndex)
        {
            case 0: Interrupt.Deactivate(EXTI0_IRQn); break;
            case 1: Interrupt.Deactivate(EXTI1_IRQn); break;
            case 2: Interrupt.Deactivate(EXTI2_IRQn); break;
            case 3: Interrupt.Deactivate(EXTI3_IRQn); break;
            case 4: Interrupt.Deactivate(EXTI4_IRQn); break;
            case 5:
            case 6:
            case 7:
            case 8:
            case 9:
                // 最后一个中断销毁才关闭中断
                if(!state->Used) Interrupt.Deactivate(EXTI9_5_IRQn); break;
            case 10:
            case 11:
            case 12:
            case 13:
            case 14:
            case 15:
                // 最后一个中断销毁才关闭中断
                if(!state->Used) Interrupt.Deactivate(EXTI15_10_IRQn); break;
        }*/
    }
}

/*void InputPort::SetShakeTime(byte ms)
{
	shake_time = ms;
}*/
