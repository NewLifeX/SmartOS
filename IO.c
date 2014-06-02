#include "System.h"

#ifndef BIT
#define BIT(x)	(1 << (x))
#endif

#define _RCC_AHB(PIN) (RCC_AHBENR_GPIOAEN << (PIN >> 4))

#ifdef STM32F0XX
// 打开端口
// mode=GPIO_Mode_IN/GPIO_Mode_OUT/GPIO_Mode_AF/GPIO_Mode_AN
// speed=GPIO_Speed_50MHz/GPIO_Speed_2MHz/GPIO_Speed_10MHz
// type=GPIO_OType_PP/GPIO_OType_OD
void TIO_OpenPort(Pin pin, GPIOMode_TypeDef mode, GPIOSpeed_TypeDef speed, GPIOOType_TypeDef type)
{
    GPIO_InitTypeDef p;
    
    // 初始化结构体，保险起见。为了节省代码空间，也可以不用
    GPIO_StructInit(&p);
    
    // 打开时钟
    RCC_AHBPeriphClockCmd(_RCC_AHB(pin), ENABLE);
    //RCC->AHBENR |= _RCC_AHB(pin);

    p.GPIO_Pin = _PORT(pin);
    p.GPIO_Speed = speed;
    p.GPIO_Mode = mode;
    p.GPIO_OType = type;	
    GPIO_Init(_GROUP(pin), &p);
}

// 按常用方式打开端口
void TIO_Open(Pin pin, GPIOMode_TypeDef mode)
{
    if(mode == GPIO_Mode_OUT)
        TIO_OpenPort(pin, mode, GPIO_Speed_50MHz, GPIO_OType_OD);
    else if(mode == GPIO_Mode_IN)
        TIO_OpenPort(pin, mode, GPIO_Speed_50MHz, GPIO_OType_OD);
    else if(mode == GPIO_Mode_AF)
        TIO_OpenPort(pin, mode, GPIO_Speed_50MHz, GPIO_OType_PP);
    else
        TIO_OpenPort(pin, mode, GPIO_Speed_50MHz, GPIO_OType_OD);
}

// 关闭端口。设为浮空输入
void TIO_Close(Pin pin)
{
    TIO_OpenPort(pin, GPIO_Mode_IN, GPIO_Speed_2MHz, GPIO_OType_PP);
    // 下面这个会关闭整个针脚组，不能用
    //GPIO_DeInit(_GROUP(pin));
}
#else
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
    RCC_AHBPeriphClockCmd(_RCC_AHB(pin), ENABLE);
    //RCC->APB2ENR |= _RCC_AHB(pin);

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
#endif

// 设置端口状态
void TIO_Write(Pin pin, bool value)
{
    if(value)
        GPIO_SetBits(_GROUP(pin), _PORT(pin));
    else
        GPIO_ResetBits(_GROUP(pin), _PORT(pin));
}

// 读取端口状态
bool TIO_Read(Pin pin)
{
    GPIO_TypeDef* group = _GROUP(pin);
    return (group->IDR >> (pin & 0xF)) & 1;
}

// IO端口函数初始化
void TIO_Init(TIO* this)
{
    this->Open = TIO_Open;
    this->OpenPort = TIO_OpenPort;
    this->Write = TIO_Write;
    this->Read = TIO_Read;
}
