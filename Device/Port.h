#ifndef _Port_H_
#define _Port_H_

#include "Sys.h"

#ifdef STM32F4
	#define GPIO_MAX_SPEED 100
#else
	#define GPIO_MAX_SPEED 50
#endif

/******** 端口打开关闭流程 ********/
/*
Port::Open
	#Port::Opening
	OutputPort::OnOpen
		#Port::OnOpen
		#OutputPort::OpenPin

Port::Close
	#Port::OnClose
*/


/******************************** Port ********************************/

// 端口基类
// 用于管理一个端口，通过PinBit标识该组的哪些引脚。
// 子类初始化时先通过SetPort设置端口，备份引脚状态，然后Config通过gpio结构体配置端口，端口销毁时恢复引脚状态
class Port : public Object
{
public:
	enum GPIO_AF
	{
		AF_0 = 0,
		AF_1 = 1,
		AF_2 = 2,
		AF_3 = 3,
		AF_4 = 4,
		AF_5 = 5,
		AF_6 = 6,
		AF_7 = 7
	};

    Pin		_Pin;		// 引脚
	bool	Opened;		// 是否已经打开
	byte    Index;		// 引脚自身次序编号，用于区分多引脚次序
	void*	State;		// 用户状态数据

	Port();
#ifndef TINY
	virtual ~Port();
#endif

    Port& Set(Pin pin);	// 设置引脚
	bool Empty() const;

	bool Open();
	void Close();
	void Clear();

	void AFConfig(GPIO_AF GPIO_AF) const;
	static void RemapConfig(uint param, bool sta);

    virtual bool Read() const;

	virtual String& ToStr(String& str) const;

protected:
    // 配置过程
    virtual void OnOpen();
	virtual void OnClose();

private:
	void Opening();
};

/******************************** OutputPort ********************************/

// 输出口
class OutputPort : public Port
{
public:
    byte Invert		= 2;		// 是否倒置输入输出。默认2表示自动检测
    bool OpenDrain	= false;	// 是否开漏输出
    byte Speed		= GPIO_MAX_SPEED;			// 速度

    OutputPort();
    explicit OutputPort(Pin pin);
    OutputPort(Pin pin, byte invert, bool openDrain = false, byte speed = GPIO_MAX_SPEED);

	OutputPort& Init(Pin pin, bool invert);

    void Write(bool value) const;
	// 拉高一段时间后拉低
	void Up(uint ms) const;
	void Down(uint ms) const;
	// 闪烁多次
	void Blink(uint times, uint ms) const;

	// Read/ReadInput 的区别在于，前者读输出后者读输入，在开漏输出的时候有很大区别
    virtual bool Read() const;
	bool ReadInput() const;

    static void Write(Pin pin, bool value);

    OutputPort& operator=(bool value) { Write(value); return *this; }
    OutputPort& operator=(OutputPort& port) { Write(port.Read()); return *this; }
    operator bool() const { return Read(); }

protected:
    virtual void OnOpen();
	virtual void OpenPin();

private:
};

/******************************** AlternatePort ********************************/

// 复用输出口
class AlternatePort : public OutputPort
{
public:
	AlternatePort();
    explicit AlternatePort(Pin pin);
    AlternatePort(Pin pin, byte invert, bool openDrain = false, byte speed = GPIO_MAX_SPEED);

protected:
    //virtual void OnOpen();
	virtual void OpenPin();

private:
};

/******************************** InputPort ********************************/

// 输入口
class InputPort : public Port
{
public:
    typedef enum
    {
        NOPULL	= 0x00,
        UP		= 0x01,	// 上拉电阻
        DOWN	= 0x02,	// 下拉电阻
    }PuPd;
    typedef enum
    {
        Rising	= 0x01,	// 上升沿
        Falling	= 0x02,	// 下降沿
		Both	= 0x03	// 上升下降沿
    }Trigger;

    // 读取委托
    typedef void (*IOReadHandler)(InputPort* port, bool down, void* param);

    ushort	ShakeTime	= 0;	// 设置 抖动时间。毫秒
	ushort	PressTime	= 0;	// 获取 长按时间。毫秒
    byte	Invert		= 2;	// 是否倒置输入输出。默认2表示自动检测
    bool	Floating	= true;	// 是否浮空输入
    PuPd	Pull		= UP;	// 上拉下拉电阻
	//Trigger	Mode		= Both;	// 触发模式，上升沿下降沿
	bool	HardEvent	= false;// 是否使用硬件事件。默认false

	Delegate2<InputPort&, bool>	Press;	// 按下事件

	InputPort();
    explicit InputPort(Pin pin, bool floating = true, PuPd pull = UP);
    virtual ~InputPort();

	InputPort& Init(Pin pin, bool invert);

	// 读取状态
    virtual bool Read() const;

	bool UsePress();
	void OnPress(bool down);

    operator bool() const { return Read(); }

protected:
    virtual void OnOpen();
	virtual void OnClose();

private:
	bool	_IRQ	= false;
	byte	_Value	= 0;	// 当前值
	uint	_task	= 0;	// 输入任务
	UInt64	_Start	= 0;	// 开始按下时间
	UInt64	_Last	= 0;	// 最后一次触发时间
	static void InputTask(void* param);

private:
	void OpenPin();
	void ClosePin();
	bool OnRegister();
};

/******************************** AnalogInPort ********************************/

// 模拟输入口
class AnalogInPort : public Port
{
public:
    AnalogInPort() : Port() { }
    explicit AnalogInPort(Pin pin) : Port() { Set(pin); Open(); }

protected:
    virtual void OnOpen();

private:
	void OpenPin();
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

/*
输入口防抖原理：
1，中断时，通过循环读取来避免极快的中断触发。
实际上不知道是否有效果，太快了无从测试，这样子可能导致丢失按下或弹起事件，将来考虑是否需要改进。
2，按下和弹起事件允许同时并存于_Value，然后启动一个任务来处理，任务支持毫秒级防抖延迟
3，因为_Value可能同时存在按下和弹起，任务处理时需要同时考虑两者。
*/

#endif //_Port_H_
