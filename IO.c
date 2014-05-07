#include "System.h"

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

// 获取组和针脚
#define _GROUP(PIN) ((GPIO_TypeDef *) (GPIOA_BASE + (((PIN) & (uint16_t)0xF0) << 6)))
#define _PORT(PIN) (1 << ((PIN) & (uint16_t)0x0F))
#define _PIN(PIN) ('A' + (PIN >> 4)), (PIN & 0x0F)
#define _RCC_AHB(PIN) (RCC_AHBENR_GPIOAEN << (PIN >> 4))

// 打开输出口
// speed=GPIO_Speed_50MHz/GPIO_Speed_2MHz/GPIO_Speed_10MHz, type=GPIO_OType_PP/GPIO_OType_OD
void TIO_OpenOutput(Pin pin, GPIOSpeed_TypeDef speed, GPIOOType_TypeDef type)
{
    GPIO_InitTypeDef p;
    
    // 初始化结构体，保险起见。为了节省代码空间，也可以不用
    GPIO_StructInit(&p);
    
    // 打开时钟
    RCC_AHBPeriphClockCmd(_RCC_AHB(pin), ENABLE);

    p.GPIO_Pin = _PORT(pin);
    p.GPIO_Speed = speed;
    p.GPIO_Mode = GPIO_Mode_OUT;
    p.GPIO_OType = type;	
    GPIO_Init(_GROUP(pin), &p);
}

// 打开输入口
void TIO_OpenInput(Pin pin, byte speed, byte type)
{
}

// 按常用方式打开端口
void TIO_OpenPort(Pin pin, bool isOutput)
{
    if(isOutput)
        TIO_OpenOutput(pin, GPIO_Speed_50MHz, GPIO_OType_OD);
    else
        TIO_OpenInput(pin, 0, 0);
}

void TIO_Set(Pin pin, bool value)
{
    if(value)
        GPIO_SetBits(_GROUP(pin), _PORT(pin));
    else
        GPIO_ResetBits(_GROUP(pin), _PORT(pin));
}

bool TIO_Get(Pin pin)
{
    GPIO_TypeDef* group = _GROUP(pin);
    return (group->IDR >> (pin & 0xF)) & 1;
}

void TIO_Init(TIO* this)
{
    this->OpenPort = TIO_OpenPort;
    this->OpenOutput = TIO_OpenOutput;
    this->OpenInput = TIO_OpenInput;
    this->Set = TIO_Set;
    this->Get = TIO_Get;
}
