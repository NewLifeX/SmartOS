#include "System.h"

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

// 获取组和针脚
#define _GROUP(PIN) ((GPIO_TypeDef *) (GPIOA_BASE + (((PIN) & (uint16_t)0xF0) << 6)))
#define _PORT(PIN) (1 << ((PIN) & (uint16_t)0x0F))
#define _PIN(PIN) ('A' + (PIN >> 4)), (PIN & 0x0F)
#define _RCC_AHB(PIN) (RCC_APB2ENR_IOPAEN << (PIN >> 4))

// 打开端口
// mode=GPIO_Mode_IN/GPIO_Mode_OUT/GPIO_Mode_AF/GPIO_Mode_AN
// speed=GPIO_Speed_50MHz/GPIO_Speed_2MHz/GPIO_Speed_10MHz
// type=GPIO_OType_PP/GPIO_OType_OD
void TIO_OpenPort(Pin pin, GPIOMode_TypeDef mode, GPIOSpeed_TypeDef speed)
{
    GPIO_InitTypeDef p;
    
    // 初始化结构体，保险起见。为了节省代码空间，也可以不用
    GPIO_StructInit(&p);
    
    // 打开时钟
    //RCC_AHBPeriphClockCmd(_RCC_AHB(pin), ENABLE);
    RCC->APB2ENR |= _RCC_AHB(pin);

    p.GPIO_Pin = _PORT(pin);
    p.GPIO_Speed = speed;
    p.GPIO_Mode = mode;
    GPIO_Init(_GROUP(pin), &p);
}

// 按常用方式打开端口
void TIO_Open(Pin pin, GPIOMode_TypeDef mode)
{
    TIO_OpenPort(pin, mode, GPIO_Speed_50MHz);
}

// 关闭端口。设为浮空输入
void TIO_Close(Pin pin)
{
    TIO_OpenPort(pin, GPIO_Mode_IN_FLOATING, GPIO_Speed_2MHz);
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
    this->Open = TIO_Open;
    this->OpenPort = TIO_OpenPort;
    this->Set = TIO_Set;
    this->Get = TIO_Get;
}
