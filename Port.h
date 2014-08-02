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
    ushort PinBit;      // 组内引脚位。每个引脚一个位
    Pin Pin0;           // 第一个针脚
	//bool Restore;		// 析构时是否恢复引脚的初始状态，默认false

    virtual void Config();    // 确定配置,确认用对象内部的参数进行初始化

    // 辅助函数
    static GPIO_TypeDef* IndexToGroup(byte index);
    static byte GroupToIndex(GPIO_TypeDef* group);
    static ushort IndexToBits(byte index);
    static byte BitsToIndex(ushort bits); // 最低那一个位的索引

#if DEBUG
	static bool Reserve(Pin pin, bool flag);	// 保护引脚，别的功能要使用时将会报错。返回是否保护成功
	static bool IsBusy(Pin pin);	// 引脚是否被保护
#endif

protected:
    GPIO_InitTypeDef gpio;

	Port();
	virtual ~Port();

    void SetPort(Pin pin);      // 单一引脚初始化
    void SetPort(Pin pins[], uint count);   // 用一组引脚来初始化，引脚组GPIOx由第一个引脚决定，请确保所有引脚位于同一组GPIOx
    void SetPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All);

    // 配置过程，由Config调用，最后GPIO_Init
    virtual void OnConfig();

private:
	//static ushort Reserved[8];		// 引脚保留位，记录每个引脚是否已经被保留，禁止别的模块使用
	ulong InitState;	// 备份引脚初始状态，在析构时还原
	
	void OnSetPort();
};

// 输入输出口基类
class InputOutputPort : public Port
{
public:
    uint Speed;// 速度
    bool Invert;        // 是否倒置输入输出

    bool Read(); // 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
    virtual ushort ReadGroup()    // 整组读取
    {
        return GPIO_ReadOutputData(Group);
    }

    static bool Read(Pin pin);

    operator bool() { return this != NULL; }

protected:
    InputOutputPort()
    {
        Speed = 2;
        Invert = false;
    }

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
    OutputPort(Pin pins[], uint count, bool openDrain = false, uint speed = 50) { SetPort(pins, count); Init(openDrain, speed); Config(); }
    OutputPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) { SetPort(group, pinbit); Init(); Config(); }

    void Write(bool value); // 按位值写入
    void WriteGroup(ushort value);   // 整组写入

    static void Write(Pin pin, bool value);

    OutputPort& operator=(bool value) { Write(value); return *this; }
    OutputPort& operator=(OutputPort& port) { Write(port.Read()); return *this; }
    operator bool() { return Read(); }

protected:
    OutputPort() { }

    virtual void OnConfig()
    {
        InputOutputPort::OnConfig();

#ifdef STM32F0XX
        gpio.GPIO_Mode = GPIO_Mode_OUT;
        gpio.GPIO_OType = OpenDrain ? GPIO_OType_OD : GPIO_OType_PP;
#else
        gpio.GPIO_Mode = OpenDrain ? GPIO_Mode_Out_OD : GPIO_Mode_Out_PP;
#endif
    }

    void Init(bool openDrain = false, uint speed = 50)
    {
        OpenDrain = openDrain;
        Speed = speed;
    }
};

// 复用输出口
class AlternatePort : public OutputPort
{
public:
    AlternatePort(Pin pin, bool openDrain = false, uint speed = 10) : OutputPort() { SetPort(pin); Init(openDrain, speed); Config(); }
    AlternatePort(Pin pins[], uint count, bool openDrain = false, uint speed = 10) : OutputPort() { SetPort(pins, count); Init(openDrain, speed); Config(); }
    AlternatePort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) : OutputPort() { SetPort(group, pinbit); Init(); Config(); }

protected:
    virtual void OnConfig()
    {
        InputOutputPort::OnConfig();

#ifdef STM32F0XX
        gpio.GPIO_Mode = GPIO_Mode_AF;
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
    uint ShakeTime;     // 抖动时间

    InputPort(Pin pin, bool floating = true, uint speed = 50, PuPd_TypeDef pupd = PuPd_NOPULL) { SetPort(pin); Init(floating, speed, pupd); }
    InputPort(Pin pins[], uint count, bool floating = true, uint speed = 50, PuPd_TypeDef pupd = PuPd_NOPULL) { SetPort(pins, count); Init(floating, speed, pupd); }
    InputPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) { SetPort(group, pinbit); Init(); }

    virtual ~InputPort();

    virtual ushort ReadGroup()    // 整组读取
    {
        return GPIO_ReadInputData(Group);
    }

    void Register(IOReadHandler handler);   // 注册事件

    operator bool() { return Read(); }

protected:
    // 函数命名为Init，而不作为构造函数，主要是因为用构造函数会导致再实例化一个对象，然后这个函数在那个新对象里面执行
    void Init(bool floating = true, uint speed = 50, PuPd_TypeDef pupd = PuPd_NOPULL)
    {
        Floating = floating;
        Speed = speed;

        _Registed = false;
        ShakeTime = 20;

        Config();
    }

    virtual void OnConfig()
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

private:
    bool _Registed;

    void RegisterInput(int groupIndex, int pinIndex, IOReadHandler handler);
    void UnRegisterInput(int pinIndex);
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
        gpio.GPIO_Mode = GPIO_Mode_AN;
        //gpio.GPIO_OType = !Floating ? GPIO_OType_OD : GPIO_OType_PP;
#else
        gpio.GPIO_Mode = GPIO_Mode_AIN; // 这里很不确定，需要根据实际进行调整
#endif
    }
};

#endif //_Port_H_
