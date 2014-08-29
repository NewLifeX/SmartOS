#ifndef _Port_H_
#define _Port_H_

#include "Sys.h"
#include "ADC.h"

#ifdef STM32F4
	#define GPIO_MAX_SPEED 100
#else
	#define GPIO_MAX_SPEED 50
#endif

// 端口基类
// 用于管理一组端口Group，通过PinBit标识该组的哪些引脚。
// 子类初始化时先通过SetPort设置端口，备份引脚状态，然后Config通过gpio结构体配置端口，端口销毁时恢复引脚状态
class Port
{
public:
    GPIO_TypeDef* Group;// 针脚组
    ushort PinBit;      // 组内引脚位。每个引脚一个位
    Pin Pin0;           // 第一个针脚。为了便于使用而设立，大多数情况下端口对象只管理一个针脚。
	byte PinCount;		// 针脚数
	byte GroupIndex;	// 分组

    virtual void Config();    // 确定配置,确认用对象内部的参数进行初始化

    // 辅助函数
    _force_inline static GPIO_TypeDef* IndexToGroup(byte index);
    _force_inline static byte GroupToIndex(GPIO_TypeDef* group);
    //_force_inline static ushort IndexToBits(byte index);
    //static byte BitsToIndex(ushort bits); // 最低那一个位的索引

#if DEBUG
	static bool Reserve(Pin pin, bool flag);	// 保护引脚，别的功能要使用时将会报错。返回是否保护成功
	static bool IsBusy(Pin pin);	// 引脚是否被保护
#endif

protected:
    GPIO_InitTypeDef gpio;	// 用于配置端口的结构体对象

	Port();
	virtual ~Port();

    void SetPort(Pin pin);      // 单一引脚初始化
    void SetPort(Pin pins[], uint count);   // 用一组引脚来初始化，引脚组GPIOx由第一个引脚决定，请确保所有引脚位于同一组GPIOx
    void SetPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All);

    // 配置过程，由Config调用，最后GPIO_Init
    virtual void OnConfig();
#if DEBUG
	virtual bool OnReserve(Pin pin, bool flag);
#endif

private:
#if defined(STM32F1)
	ulong InitState;	// 备份引脚初始状态，在析构时还原
#endif

	void OnSetPort();
};

// 输出口
class OutputPort : public Port
{
public:
    bool OpenDrain;  // 是否开漏输出
    uint Speed;		// 速度
    bool Invert;	// 是否倒置输入输出

    OutputPort(Pin pin, bool openDrain = false, uint speed = GPIO_MAX_SPEED) { SetPort(pin); Init(openDrain, speed); Config(); }
    OutputPort(Pin pins[], uint count, bool openDrain = false, uint speed = GPIO_MAX_SPEED) { SetPort(pins, count); Init(openDrain, speed); Config(); }
    OutputPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) { SetPort(group, pinbit); Init(); Config(); }

    void Write(bool value); // 按位值写入
    void WriteGroup(ushort value);   // 整组写入
	void Up(uint ms);	// 拉高一段时间后拉低
	void Blink(uint times, uint ms);	// 闪烁多次

    ushort ReadGroup();    // 整组读取
    bool Read();	// 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
    static bool Read(Pin pin);
    static void Write(Pin pin, bool value);

    OutputPort& operator=(bool value) { Write(value); return *this; }
    OutputPort& operator=(OutputPort& port) { Write(port.Read()); return *this; }
    operator bool() { return Read(); }

protected:
    OutputPort() { }

    virtual void OnConfig();

    void Init(bool openDrain = false, uint speed = GPIO_MAX_SPEED)
    {
		OutputPort();
        OpenDrain = openDrain;
        Speed = speed;
        Invert = false;
    }

#if DEBUG
	virtual bool OnReserve(Pin pin, bool flag);
#endif
};

// 复用输出口
class AlternatePort : public OutputPort
{
public:
    AlternatePort(Pin pin, bool openDrain = false, uint speed = GPIO_MAX_SPEED) : OutputPort() { SetPort(pin); Init(openDrain, speed); Config(); }
    AlternatePort(Pin pins[], uint count, bool openDrain = false, uint speed = GPIO_MAX_SPEED) : OutputPort() { SetPort(pins, count); Init(openDrain, speed); Config(); }
    AlternatePort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) : OutputPort() { SetPort(group, pinbit); Init(); Config(); }

protected:
    virtual void OnConfig();

#if DEBUG
	virtual bool OnReserve(Pin pin, bool flag);
#endif
};

// 输入口
class InputPort : public Port
{
public:
    typedef enum
    {
        PuPd_NOPULL = 0x00,
        PuPd_UP     = 0x01,
        PuPd_DOWN   = 0x02
    }PuPd_TypeDef;

    // 读取委托
    typedef void (*IOReadHandler)(Pin pin, bool down, void* param);

    PuPd_TypeDef PuPd;  // 上拉下拉电阻
    bool Floating;      // 是否浮空输入
    uint ShakeTime;     // 抖动时间
    bool Invert;		// 是否倒置输入输出

    InputPort(Pin pin, bool floating = true, PuPd_TypeDef pupd = PuPd_UP) { SetPort(pin); Init(floating, pupd); }
    InputPort(Pin pins[], uint count, bool floating = true, PuPd_TypeDef pupd = PuPd_UP) { SetPort(pins, count); Init(floating, pupd); }
    InputPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) { SetPort(group, pinbit); Init(); }

    virtual ~InputPort();

    ushort ReadGroup();    // 整组读取
    bool Read();	// 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
    static bool Read(Pin pin);

    void Register(IOReadHandler handler, void* param = NULL);   // 注册事件

    operator bool() { return Read(); }

protected:
    // 函数命名为Init，而不作为构造函数，主要是因为用构造函数会导致再实例化一个对象，然后这个函数在那个新对象里面执行
    void Init(bool floating = true, PuPd_TypeDef pupd = PuPd_UP)
    {
		PuPd = pupd;
        Floating = floating;

        _Registed = false;
        ShakeTime = 20;
        Invert = false;

        Config();
    }

    virtual void OnConfig();

#if DEBUG
	virtual bool OnReserve(Pin pin, bool flag);
#endif

private:
    bool _Registed;

    void RegisterInput(int groupIndex, int pinIndex, IOReadHandler handler, void* param);
    void UnRegisterInput(int pinIndex);
};

// 模拟输入输出口
class AnalogInPort : public Port
{
public:
    AnalogInPort(Pin pin) { SetPort(pin); Config(); }
    AnalogInPort(Pin pins[], uint count) { SetPort(pins, count); Config(); }
	AnalogInPort(GPIO_TypeDef* group, ushort pinbit = GPIO_Pin_All) { SetPort(group, pinbit); Config(); }

protected:
    virtual void OnConfig();
};

#endif //_Port_H_
