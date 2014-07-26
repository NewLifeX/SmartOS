#ifndef _Port_H_
#define _Port_H_

#include "Sys.h"

#ifdef STM32F10X
	#include "stm32f10x_gpio.h"
#else
	#include "stm32f0xx_gpio.h"
#endif

// 端口类
class Port
{
public:
    GPIO_TypeDef* Group;// 针脚组
    ushort PinBit;       // 组内引脚位。每个引脚一个位

    virtual void Config();    // 确定配置,确认用对象内部的参数进行初始化

    // 辅助函数
    static GPIO_TypeDef* IndexToGroup(byte index);
    static byte GroupToIndex(GPIO_TypeDef* group);
    static ushort IndexToBits(byte index);
    static byte BitsToIndex(byte bits); // 最低那一个位的索引

    //Port& operator=(bool value) { Write(value); return *this; }
    //Port& operator=(Port& port) { Write(port.Read()); return *this; }
protected:
    GPIO_InitTypeDef gpio;

    Port()
    {
        // 特别要慎重，有些结构体成员可能因为没有初始化而酿成大错
        GPIO_StructInit(&gpio);
    }

    void SetPort(Pin pin);      // 单一引脚初始化
    void SetPort(Pin pins[]);   // 用一组引脚来初始化，引脚组GPIOx由第一个引脚决定，请确保所有引脚位于同一组GPIOx
    void SetPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All);

    // 配置过程，由Config调用，最后GPIO_Init
    virtual void OnConfig();

    typedef enum
    {
        Mode_IN   = 0x00, /*!< GPIO Input Mode              */
        Mode_OUT  = 0x01, /*!< GPIO Output Mode             */
        Mode_AF   = 0x02, /*!< GPIO Alternate function Mode */
        Mode_AN   = 0x03  /*!< GPIO Analog In/Out Mode      */
    }Mode_TypeDef;

    Mode_TypeDef Mode;  // 模式
};

// 输入输出口基类
class InputOutputPort : public Port
{
public:
    /*typedef enum
    { 
      Speed_10MHz = 1,
      Speed_2MHz, 
      Speed_50MHz
    }Speed_TypeDef;*/

    uint Speed;// 速度

    bool Invert;        // 是否倒置输入输出

    bool Read(); // 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
    ushort ReadGroup();    // 整组读取

    static bool Read(Pin pin);

    operator bool() { return this != NULL; }

protected:
    virtual void OnConfig()
    {
        Port::OnConfig();

        switch(Speed)
        {
            case 2: gpio.GPIO_Speed = GPIO_Speed_2MHz; break;
            case 10: gpio.GPIO_Speed = GPIO_Speed_10MHz; break;
            case 50: gpio.GPIO_Speed = GPIO_Speed_50MHz; break;
        }
    }
};

// 输出口
class OutputPort : public InputOutputPort
{
public:
    bool OpenDrain;  // 是否开漏输出

    OutputPort(Pin pin, bool openDrain = false, uint speed = 50) { SetPort(pin); Init(openDrain, speed); Config(); }
    OutputPort(Pin pins[], bool openDrain = false, uint speed = 50) { SetPort(pins); Init(openDrain, speed); Config(); }
    OutputPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) { SetPort(group, pinbit); Init(); Config(); }

    void Write(bool value); // 按位值写入
    void WriteGroup(ushort value);   // 整组写入

    static void Write(Pin pin, bool value);

    OutputPort& operator=(bool value) { Write(value); return *this; }
    OutputPort& operator=(OutputPort& port) { Write(port.Read()); return *this; }
    operator bool() { return this != NULL; }

protected:
    void Init(bool openDrain = false, uint speed = 50)
    {
        // 默认初始化为50M推挽输出
        Mode = Port::Mode_OUT;
        OpenDrain = openDrain;
        Speed = speed;
    }

    virtual void OnConfig()
    {
        InputOutputPort::OnConfig();

#ifdef STM32F0XX
        gpio.GPIO_Mode = Mode_OUT;
        gpio.GPIO_OType = OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;	
#else
        gpio.GPIO_Mode = OpenDrain ? GPIO_Mode_Out_OD : GPIO_Mode_Out_PP;
#endif
    }
};

// 复用输出口
class AlternatePort : public OutputPort
{
public:
    AlternatePort(Pin pin, bool openDrain = false, uint speed = 10) : OutputPort(pin, openDrain, speed) { Init(); Config(); }
    AlternatePort(Pin pins[], bool openDrain = false, uint speed = 10) : OutputPort(pins, openDrain, speed) { Init(); Config(); }
    AlternatePort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) : OutputPort(group, pinbit) { Init(); Config(); }

protected:
    void Init()
    {
        Mode = Port::Mode_AF;
        OpenDrain = false;
        Speed = 10;
    }

    virtual void OnConfig()
    {
        InputOutputPort::OnConfig();

#ifdef STM32F0XX
        gpio.GPIO_Mode = Mode_AF;
        gpio.GPIO_OType = OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;	
#else
        gpio.GPIO_Mode = OpenDrain ? GPIO_Mode_AF_OD : GPIO_Mode_AF_PP;
#endif
    }
};

// 输入口
class InputPort : public InputOutputPort
{
public:
    typedef enum
    {
        PuPd_NOPULL = 0x00,
        PuPd_UP     = 0x01,
        PuPd_DOWN   = 0x02
    }PuPd_TypeDef;

    // 读取委托
    typedef void (*IOReadHandler)(Pin , bool );

    PuPd_TypeDef PuPd;  // 上拉下拉电阻
    bool Floating;      // 是否浮空输入

    InputPort(Pin pin, bool floating = true, uint speed = 50, PuPd_TypeDef pupd = PuPd_NOPULL) { SetPort(pin); Init(floating, speed, pupd); }
    InputPort(Pin pins[], bool floating = true, uint speed = 50, PuPd_TypeDef pupd = PuPd_NOPULL) { SetPort(pins); Init(floating, speed, pupd); }
    InputPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) { SetPort(group, pinbit); Init(); }

    void Register(IOReadHandler handler);   // 申请引脚中断托管
    static void SetShakeTime(byte ms);			//设置按键去抖动时间

    operator bool() { return Read(); }

protected:
    void Init(bool floating = true, uint speed = 50, PuPd_TypeDef pupd = PuPd_NOPULL)
    {
        Mode = Port::Mode_IN;
        Floating = floating;
        Speed = speed;

        Config();
    }

    virtual void OnConfig()
    {
        InputOutputPort::OnConfig();

#ifdef STM32F0XX
        gpio.GPIO_Mode = Mode_IN;
        //gpio.GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;	
#else
        gpio.GPIO_Mode = Floating ? GPIO_Mode_IN_FLOATING : GPIO_Mode_IPD; // 这里很不确定，需要根据实际进行调整
#endif
    }
};

// 模拟输入输出口
class AnalogPort : public Port
{
public:
    AnalogPort(Pin pin) { SetPort(pin); }
    //AnalogPort(Pin pins[]) : Port(pins) { OnInit(); }
    AnalogPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) { SetPort(group, pinbit); }

protected:
    virtual void OnConfig()
    {
        Port::OnConfig();

#ifdef STM32F0XX
        gpio.GPIO_Mode = Mode_AN;
        //gpio.GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;	
#else
        gpio.GPIO_Mode = GPIO_Mode_AIN; // 这里很不确定，需要根据实际进行调整
#endif
    }
};

#endif //_Port_H_
