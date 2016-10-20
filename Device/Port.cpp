#include "Sys.h"
#include "Port.h"

/******************************** Port ********************************/

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port
Port::Port()
{
	_Pin	= P0;
	Group	= nullptr;
	Mask	= 0;
	Opened	= false;
}

#ifndef TINY
Port::~Port()
{
	Close();
}
#endif

String& Port::ToStr(String& str) const
{
	str	+= 'P';
	if(_Pin == P0)
	{
		str	+= '0';
	}
	else
	{
		str	+= (char)('A' + (_Pin >> 4));
		str	+= (byte)(_Pin & 0x0F);
	}
	return str;
}

// 单一引脚初始化
Port& Port::Set(Pin pin)
{
	// 如果引脚不变，则不做处理
	if(pin == _Pin) return *this;

#ifndef TINY
	// 释放已有引脚的保护
	if(_Pin != P0) Close();
#endif

    _Pin = pin;
	if(_Pin != P0)
	{
		Group	= IndexToGroup(pin >> 4);
		Mask	= 1 << (pin & 0x0F);
	}
	else
	{
		Group	= nullptr;
		Mask	= 0;
	}

	return *this;
}

bool Port::Empty() const
{
	if(_Pin != P0) return false;

	if(Group == nullptr || Mask == 0) return true;

	return false;
}

void Port::Clear()
{
	Group	= nullptr;
	_Pin	= P0;
	Mask	= 0;
}

// 分组时钟
static byte _GroupClock[10];

void Port::OpenClock(Pin pin, bool flag)
{
    int gi = pin >> 4;

	if(flag)
	{
		// 增加计数，首次打开时钟
		if(_GroupClock[gi]++) return;
	}
	else
	{
		// 减少计数，最后一次关闭时钟
		if(_GroupClock[gi]-- > 1) return;
	}

	OnOpenClock(pin, flag);
}

// 确定配置,确认用对象内部的参数进行初始化
bool Port::Open()
{
	if(_Pin == P0) return false;
	if(Opened) return true;

	TS("Port::Open");

    // 先打开时钟才能配置
	OpenClock(_Pin, true);

#if DEBUG
	// 保护引脚
	//Show();
	GetType().Name().Show();
	Reserve(_Pin, true);
#endif

	OpenPin();

	Opened = true;

	return true;
}

void Port::Close()
{
	if(!Opened) return;
	if(_Pin == P0) return;

	OnClose();

#if DEBUG
	// 保护引脚
	//Show();
	GetType().Name().Show();
	Reserve(_Pin, false);
	debug_printf("\r\n");
#endif

	Opened = false;
}

void Port::OnClose()
{
	// 不能随便关闭时钟，否则可能会影响别的引脚
	OpenClock(_Pin, false);
}
#endif

// 端口引脚保护
#if DEBUG
// 引脚保留位，记录每个引脚是否已经被保留，禁止别的模块使用
// !!!注意，不能全零，否则可能会被当作zeroinit而不是copy，导致得不到初始化
static ushort Reserved[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF};

// 保护引脚，别的功能要使用时将会报错。返回是否保护成功
bool Port::Reserve(Pin pin, bool flag)
{
	debug_printf("::");
    int port = pin >> 4, bit = 1 << (pin & 0x0F);
    if (flag) {
        if (Reserved[port] & bit) {
			// 增加针脚已经被保护的提示，很多地方调用ReservePin而不写日志，得到False后直接抛异常
			debug_printf("P%c%d 已被打开", _PIN_NAME(pin));
			return false; // already reserved
		}
        Reserved[port] |= bit;

		debug_printf("打开 P%c%d", _PIN_NAME(pin));
    } else {
        Reserved[port] &= ~bit;

		debug_printf("关闭 P%c%d", _PIN_NAME(pin));
	}

    return true;
}

// 引脚是否被保护
bool Port::IsBusy(Pin pin)
{
    int port = pin >> 4, sh = pin & 0x0F;
    return (Reserved[port] >> sh) & 1;
}
#endif

/******************************** OutputPort ********************************/

// 输出端口
#define REGION_Output 1
#ifdef REGION_Output

OutputPort::OutputPort() : Port() { }
OutputPort::OutputPort(Pin pin) : OutputPort(pin, 2) { }
OutputPort::OutputPort(Pin pin, byte invert, bool openDrain, byte speed) : Port()
{
	OpenDrain	= openDrain;
	Speed		= speed;
	Invert		= invert;

	if(pin != P0)
	{
		Set(pin);
		Open();
	}
}

OutputPort& OutputPort::Init(Pin pin, bool invert)
{
	Port::Set(pin);

	Invert	= invert;

	return *this;
}

void OutputPort::OnOpen(void* param)
{
	TS("OutputPort::OnOpen");

#if DEBUG
	debug_printf(" %dM", Speed);
	if(OpenDrain)
		debug_printf(" 开漏");
	else
		debug_printf(" 推挽");
	bool fg	= false;
#endif

	// 根据倒置情况来获取初始状态，自动判断是否倒置
	bool rs = Port::Read();
	if(Invert > 1)
	{
		Invert	= rs;
#if DEBUG
		fg		= true;
#endif
	}

#if DEBUG
	if(Invert)
	{
		if(fg)
			debug_printf(" 自动倒置");
		else
			debug_printf(" 倒置");
	}
#endif

#if DEBUG
	debug_printf(" 初始电平=%d \r\n", rs);
#endif

	Port::OnOpen(param);

	OpenPin(param);
}

void OutputPort::Up(uint ms) const
{
	if(Empty()) return;

    Write(true);
	Sys.Sleep(ms);
    Write(false);
}

void OutputPort::Down(uint ms) const
{
	if(Empty()) return;

    Write(false);
	Sys.Sleep(ms);
    Write(true);
}

void OutputPort::Blink(uint times, uint ms) const
{
	if(Empty()) return;

	bool flag = true;
    for(int i=0; i<times; i++)
	{
		Write(flag);
		flag = !flag;
		Sys.Sleep(ms);
	}
    Write(false);
}

/******************************** AlternatePort ********************************/

AlternatePort::AlternatePort() : OutputPort(P0, false, false) { }
AlternatePort::AlternatePort(Pin pin) : OutputPort(pin, false, false) { }
AlternatePort::AlternatePort(Pin pin, byte invert, bool openDrain, byte speed)
	: OutputPort(pin, invert, openDrain, speed) { }

void AlternatePort::OnOpen(void* param)
{
	OutputPort::OnOpen(param);

	OpenPin(param);
}

#endif

/******************************** InputPort ********************************/

// 输入端口
#define REGION_Input 1
#ifdef REGION_Input

InputPort::InputPort() : InputPort(P0) { }
InputPort::InputPort(Pin pin, bool floating, PuPd pull) : Port()
{
	_task	= 0;

	//Handler	= nullptr;
	//Param	= nullptr;
	_Value	= 0;

	_Start	= 0;
	PressTime	= 0;
	_Last	= 0;

	if(pin != P0)
	{
		Set(pin);
		Open();
	}
}

InputPort::~InputPort()
{
	Sys.RemoveTask(_task);
}

InputPort& InputPort::Init(Pin pin, bool invert)
{
	Port::Set(pin);

	Invert	= invert;

	return *this;
}

// 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
bool InputPort::Read() const
{
	return Port::Read() ^ Invert;
}

void InputPort::OnPress(bool down)
{
	//debug_printf("OnPress P%c%d down=%d Invert=%d _Value=%d\r\n", _PIN_NAME(_Pin), down, Invert, _Value);
	/*
	！！！注意：
	有些按钮可能会出现110现象，也就是按下的时候1（正常），弹起的时候连续的1和0（不正常）。
	*/

	UInt64	now	= Sys.Ms();
	// 这一次触发离上一次太近，算作抖动忽略掉
	if(_Last > 0 && ShakeTime > 0 && now - _Last < ShakeTime) return;
	_Last	= now;
	
	// 状态机。上一次和这一次状态相同时，认为出错，抛弃
	if(down && _Value == Rising) return;
	if(!down && _Value != Rising) return;
	_Value	= down ? Rising : Falling;

	if(down)
		_Start	= now;
	else
		PressTime	= now - _Start;

	if(down)
	{
		if((Mode & Rising) == 0) return;
	}
	else
	{
		if((Mode & Falling) == 0) return;
	}

	if(HardEvent)
	{
		//if(Handler) Handler(this, down, Param);
		Press(*this, down);
	}
	else
	{
		// 允许两个值并存
		//_Value	|= down ? Rising : Falling;
		Sys.SetTask(_task, true, 0);
	}
}

void InputPort::InputTask(void* param)
{
	auto port	= (InputPort*)param;
	byte v	= port->_Value;
	if(!v) return;

	/*if(port->Handler)
	{
		//port->_Value = 0;
		v	&= port->Mode;
		if(v & Rising)	port->Handler(port, true, port->Param);
		if(v & Falling)	port->Handler(port, false, port->Param);
	}*/
	v	&= port->Mode;
	if(v & Rising)	port->Press(*port, true);
	if(v & Falling)	port->Press(*port, false);
}

void InputPort::OnOpen(void* param)
{
	TS("InputPort::OnOpen");

	// 如果不是硬件事件，则默认使用20ms抖动
	if(!HardEvent && ShakeTime == 0) ShakeTime = 20;
#if DEBUG
	debug_printf(" 抖动=%dms", ShakeTime);
	if(Floating)
		debug_printf(" 浮空");
	else if(Pull == UP)
		debug_printf(" 上升沿");
	else if(Pull == DOWN)
		debug_printf(" 下降沿");
	if(Mode & Rising)	debug_printf(" 按下");
	if(Mode & Falling)	debug_printf(" 弹起");

	bool fg	= false;
#endif

	// 根据倒置情况来获取初始状态，自动判断是否倒置
	bool rs = Port::Read();
	if(Invert > 1)
	{
		Invert	= rs;
#if DEBUG
		fg		= true;
#endif
	}

#if DEBUG
	if(Invert)
	{
		if(fg)
			debug_printf(" 自动倒置");
		else
			debug_printf(" 倒置");
	}
#endif

#if DEBUG
	debug_printf(" 初始电平=%d \r\n", rs);
#endif

	Port::OnOpen(param);
	OpenPin(param);
}

void InputPort::OnClose()
{
	Port::OnClose();

	ClosePin();

	//RemoveFromCenter(this);
}

// 注册回调  及中断使能
/*bool InputPort::Register(IOReadHandler handler, void* param)
{
	assert(_Pin != P0, "输入注册必须先设置引脚");

    Handler	= handler;
	Param	= param;

    if(!OnRegister()) return false;

	if (!_task && !HardEvent) _task	= Sys.AddTask(InputTask, this, -1, -1, "输入中断");
	//_task	= Sys.AddTask(InputTask, this, 3000, 3000, "输入中断");

	return true;
}*/

bool InputPort::Register(const Delegate2<InputPort&, bool>& dlg)
{
	assert(_Pin != P0, "输入注册必须先设置引脚");

    //Handler	= handler;
	//Param	= param;

    if(!OnRegister()) return false;

	Press	= dlg;

	if (!_task && !HardEvent) _task	= Sys.AddTask(InputTask, this, -1, -1, "输入中断");
	//_task	= Sys.AddTask(InputTask, this, 3000, 3000, "输入中断");

	return true;
}

#endif

/******************************** AnalogInPort ********************************/

void AnalogInPort::OnOpen(void* param)
{
#if DEBUG
	debug_printf("\r\n");
#endif

	Port::OnOpen(param);

	OpenPin(param);
}
