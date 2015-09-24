#ifndef _Port_H_
#define _Port_H_

#include "Sys.h"

#ifdef STM32F4
	#define GPIO_MAX_SPEED 100
#else
	#define GPIO_MAX_SPEED 50
#endif

/******************************** Port ********************************/

// 端口基类
// 用于管理一个端口，通过PinBit标识该组的哪些引脚。
// 子类初始化时先通过SetPort设置端口，备份引脚状态，然后Config通过gpio结构体配置端口，端口销毁时恢复引脚状态
class Port : public Object
{
public:
    GPIO_TypeDef*	Group;		// 针脚组
    Pin				_Pin;		// 针脚
    ushort			PinBit;		// 组内引脚位。每个引脚一个位
	bool			Opened;		// 是否已经打开

    Port& Set(Pin pin);			// 设置引脚
	bool Empty() const;

	bool Open();
	void Close();
	void Clear();

#if defined(STM32F0) || defined(STM32F4)
	void AFConfig(byte GPIO_AF) const;
#endif

    // 辅助函数
    _force_inline static GPIO_TypeDef* IndexToGroup(byte index);
    _force_inline static byte GroupToIndex(GPIO_TypeDef* group);

#if DEBUG
	static bool Reserve(Pin pin, bool flag);	// 保护引脚，别的功能要使用时将会报错。返回是否保护成功
	static bool IsBusy(Pin pin);	// 引脚是否被保护
#endif

protected:
	Port();
	virtual ~Port();

    // 配置过程，由Open调用，最后GPIO_Init
    virtual void OnOpen(GPIO_InitTypeDef& gpio);
	virtual void OnClose();
};

/******************************** OutputPort ********************************/

// 输出口
class OutputPort : public Port
{
public:
    bool OpenDrain;	// 是否开漏输出
    bool Invert;	// 是否倒置输入输出
    uint Speed;		// 速度

    OutputPort() : Port() { Init(); }
    OutputPort(Pin pin, bool invert = false, bool openDrain = false, uint speed = GPIO_MAX_SPEED) : Port()
	{
		Init(invert, openDrain, speed);
		Set(pin);
		Open();
	}

	OutputPort& Init(Pin pin, bool invert);

	// 整体写入所有包含的引脚
    void Write(bool value);
    void WriteGroup(ushort value);   // 整组写入
	void Up(uint ms);	// 拉高一段时间后拉低
	void Blink(uint times, uint ms);	// 闪烁多次

    ushort ReadGroup();    // 整组读取
	// 读取指定索引引脚。索引按照从小到大，0xFF表示任意脚为true则返回true
    bool Read(byte index);
    bool Read();		// Read() ReadReal() 的区别在  前者读输出  一个读输入    在开漏输出的时候有很大区别
	bool ReadInput();

    static bool Read(Pin pin);
    static void Write(Pin pin, bool value);

    OutputPort& operator=(bool value) { Write(value); return *this; }
    OutputPort& operator=(OutputPort& port) { Write(port.Read()); return *this; }
    operator bool() { return Read(); }

protected:
    virtual void OnOpen(GPIO_InitTypeDef& gpio);

    void Init(bool invert = false, bool openDrain = false, uint speed = GPIO_MAX_SPEED)
    {
        OpenDrain	= openDrain;
        Speed		= speed;
        Invert		= invert;
    }
};

/******************************** AlternatePort ********************************/

// 复用输出口
class AlternatePort : public OutputPort
{
public:
	AlternatePort() : OutputPort() { Init(false, false); }
    AlternatePort(Pin pin, bool invert = false, bool openDrain = false, uint speed = GPIO_MAX_SPEED)
		: OutputPort()
	{
		Init(invert, openDrain, speed);
		Set(pin);
		Open();
	}

protected:
    virtual void OnOpen(GPIO_InitTypeDef& gpio);
};

/******************************** InputPort ********************************/

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

    uint ShakeTime;     // 抖动时间
    PuPd_TypeDef PuPd;  // 上拉下拉电阻
    bool Floating;      // 是否浮空输入
    bool Invert;		// 是否倒置输入输出

	bool HardEvent;		// 是否使用硬件事件。默认false
	byte Mode;			// 触发模式，避免事件被覆盖。0x01按下，0x02弹起，默认0x03

	InputPort() : Port() { Init(); }
    InputPort(Pin pin, bool floating = true, PuPd_TypeDef pupd = PuPd_UP) : Port()
	{
		Init(floating, pupd);
		Set(pin);
		Open();
	}

    virtual ~InputPort();

	InputPort& Init(Pin pin, bool invert);

    ushort ReadGroup() const;			// 整组读取
    bool Read() const;				// 读取状态
    static bool Read(Pin pin);	// 读取某个引脚

    void Register(IOReadHandler handler, void* param = NULL);   // 注册事件

    operator bool() { return Read(); }

	void OnPress(bool down);

protected:
    void Init(bool floating = true, PuPd_TypeDef pupd = PuPd_UP);

    virtual void OnOpen(GPIO_InitTypeDef& gpio);
	virtual void OnClose();

private:
    IOReadHandler	Handler;
	void*			Param;

	bool _Value;
	uint _taskInput;		// 输入任务
	static void InputTask(void* param);
};

/******************************** AnalogInPort ********************************/

// 模拟输入输出口
class AnalogInPort : public Port
{
public:
    AnalogInPort(Pin pin) : Port() { Set(pin); Open(); }

protected:
    virtual void OnOpen(GPIO_InitTypeDef& gpio);
};

/******************************** PortScope ********************************/

// 输出端口会话类。初始化时打开端口，超出作用域析构时关闭。反向操作可配置端口为倒置
class PortScope
{
private:
	OutputPort* _port;
	bool _value;

public:
	PortScope(OutputPort* port, bool value = true)
	{
		_port = port;
		if(_port)
		{
			// 备份数值，析构的时候需要还原
			_value = port->Read();
			*_port = value;
		}
	}

	~PortScope()
	{
		if(_port) *_port = _value;
	}
};

#endif //_Port_H_
