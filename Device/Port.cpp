#include "Kernel\Sys.h"
#include "Device\Port.h"

#include <typeinfo>
using namespace ::std;

/******************************** Port ********************************/

#if DEBUG
// 保护引脚，别的功能要使用时将会报错。返回是否保护成功
static bool Port_Reserve(Pin pin, bool flag);
#endif

// 端口基本功能
#define REGION_Port 1
#ifdef REGION_Port
Port::Port()
{
	_Pin = P0;
	Opened = false;
	Index = 0;
	State = nullptr;
}

#ifndef TINY
Port::~Port()
{
	Close();
}
#endif

String Port::ToString() const
{
	String str;
	str += 'P';
	if (_Pin == P0)
	{
		str += '0';
	}
	else
	{
		str += (char)('A' + (_Pin >> 4));
		str += (byte)(_Pin & 0x0F);
	}
	return str;
}

// 单一引脚初始化
Port& Port::Set(Pin pin)
{
	// 如果引脚不变，则不做处理
	if (pin == _Pin) return *this;

#ifndef TINY
	// 释放已有引脚的保护
	if (_Pin != P0) Close();
#endif

	_Pin = pin;

	return *this;
}

bool Port::Empty() const
{
	if (_Pin != P0) return false;

	return true;
}

void Port::Clear()
{
	_Pin = P0;
}

// 确定配置,确认用对象内部的参数进行初始化
bool Port::Open()
{
	if (_Pin == P0) return false;
	if (Opened) return true;

	TS("Port::Open");

#if DEBUG
	// 保护引脚
	auto name = typeid(*this).name();
	while (*name >= '0' && *name <= '9') name++;
	debug_printf("%s", name);
	Port_Reserve(_Pin, true);
#endif

	Opening();

	Opened = true;

	return true;
}

void Port::Close()
{
	if (!Opened) return;
	if (_Pin == P0) return;

	OnClose();

#if DEBUG
	// 保护引脚
	auto name = typeid(*this).name();
	while (*name >= '0' && *name <= '9') name++;
	debug_printf("%s", name);
	Port_Reserve(_Pin, false);
	debug_printf("\r\n");
#endif

	Opened = false;
}

WEAK void Port::Opening() { OnOpen(nullptr); }
WEAK void Port::OnOpen(void* param) {}

WEAK void Port::OnClose() {}

WEAK void Port::RemapConfig(uint param, bool sta) {}
WEAK void Port::AFConfig(GPIO_AF GPIO_AF) const {}
#endif

// 端口引脚保护
#if DEBUG
// 引脚保留位，记录每个引脚是否已经被保留，禁止别的模块使用
// !!!注意，不能全零，否则可能会被当作zeroinit而不是copy，导致得不到初始化
static ushort Reserved[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF };

// 保护引脚，别的功能要使用时将会报错。返回是否保护成功
bool Port_Reserve(Pin pin, bool flag)
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
	}
	else {
		Reserved[port] &= ~bit;

		debug_printf("关闭 P%c%d", _PIN_NAME(pin));
	}

	return true;
}

/*// 引脚是否被保护
bool Port::IsBusy(Pin pin)
{
	int port = pin >> 4, sh = pin & 0x0F;
	return (Reserved[port] >> sh) & 1;
}*/
#endif

/******************************** OutputPort ********************************/

// 输出端口
#define REGION_Output 1
#ifdef REGION_Output

OutputPort::OutputPort() : Port() { }
OutputPort::OutputPort(Pin pin) : OutputPort(pin, 2) { }
OutputPort::OutputPort(Pin pin, byte invert, bool openDrain, byte speed) : Port()
{
	OpenDrain = openDrain;
	Speed = speed;
	Invert = invert;

	if (pin != P0)
	{
		Set(pin);
		Open();
	}
}

OutputPort& OutputPort::Init(Pin pin, bool invert)
{
	Port::Set(pin);

	Invert = invert;

	return *this;
}

void OutputPort::OnOpen(void* param)
{
	TS("OutputPort::OnOpen");

#if DEBUG
	debug_printf(" %dM", Speed);
	if (OpenDrain)
		debug_printf(" 开漏");
	else
		debug_printf(" 推挽");
	bool fg = false;
#endif

	// 根据倒置情况来获取初始状态，自动判断是否倒置
	bool rs = Port::Read();
	if (Invert > 1)
	{
		Invert = rs;
#if DEBUG
		fg = true;
#endif
	}

#if DEBUG
	if (Invert)
	{
		if (fg)
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

WEAK bool OutputPort::ReadInput() const
{
	if (Empty()) return false;

	bool v = Port::Read();
	if (Invert)return !v;
	return v;
}

void OutputPort::Up(int ms) const
{
	if (Empty()) return;

	Write(true);
	Sys.Sleep(ms);
	Write(false);
}

void OutputPort::Down(int ms) const
{
	if (Empty()) return;

	Write(false);
	Sys.Sleep(ms);
	Write(true);
}

void OutputPort::Blink(int times, int ms) const
{
	if (Empty()) return;

	bool flag = true;
	for (int i = 0; i < times; i++)
	{
		Write(flag);
		flag = !flag;
		Sys.Sleep(ms);
	}
	Write(false);
}

/******************************** AlternatePort ********************************/

AlternatePort::AlternatePort() : OutputPort(P0, false, false) { }
AlternatePort::AlternatePort(Pin pin) : OutputPort(P0, false, false)
{
	if (pin != P0)
	{
		Set(pin);
		Open();
	}
}
AlternatePort::AlternatePort(Pin pin, byte invert, bool openDrain, byte speed)
	: OutputPort(P0, invert, openDrain, speed)
{
	if (pin != P0)
	{
		Set(pin);
		Open();
	}
}

WEAK void AlternatePort::OpenPin(void* param) { OutputPort::OpenPin(param); }

#endif

/******************************** InputPort ********************************/

// 输入端口
#define REGION_Input 1
#ifdef REGION_Input

InputPort::InputPort() : InputPort(P0) { }
InputPort::InputPort(Pin pin, bool floating, PuPd pull) : Port()
{
	_task = 0;

	_Value = 0;
	//_qu_read = 0;
	//_qu_write = 0;

	_Start = 0;
	PressTime = 0;
	_Last = 0;

	if (pin != P0)
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

	Invert = invert;

	return *this;
}

// 读取本组所有引脚，任意脚为true则返回true，主要为单一引脚服务
bool InputPort::Read() const
{
	bool v = Port::Read();
	if (Invert)return !v;
	return v;
}

#if DEBUG
int InputPort_Total = 0;
int InputPort_Error = 0;
#endif
void InputPort::OnPress(bool down)
{
	// 在GD32F103VE上，按下PE13，有5%左右几率触发PE14的弹起中断
	if (_LastValue == down)return;
	_LastValue = down;

	/*
	！！！注意：
	有些按钮可能会出现110现象，也就是按下的时候1（正常），弹起的时候连续的1和0（不正常）。
	*/

#if DEBUG
	InputPort_Total++;
#endif
	int	now = (int)Sys.Ms();
	// 这一次触发离上一次太近，算作抖动忽略掉
	//if (_Last > 0 && ShakeTime > 0 && now - _Last < ShakeTime) return;
	if (_Last > 0 && ShakeTime > 0 && now - _Last < ShakeTime)
	{
		// 撤消上一次准备执行的动作，并取消这一次事件
		_Value = 0;
#if DEBUG
		InputPort_Error++;
#endif
		return;
	}
	if (Index == 1)
	{
		_Last = now;
	}
	_Last = now;

	// 允许两个值并存
	_Value = down ? Rising : Falling;
	/*// 压入队列
	_queue[_qu_write++] = down ? Rising : Falling;
	if (_qu_write >= sizeof(_queue)) _qu_write = 0;
	// 如果追上了读取指针，那么说明溢出
	if (_qu_write == _qu_read)
	{
		_qu_write--;
		debug_printf("InputPort::OnPress 队列溢出，共有%d个输入状态未处理 \r\n", sizeof(_queue));
	}
	int n = _qu_write - _qu_read;
	if (n < 0) n += sizeof(_queue);
	Port_Cur = n;
	if (n > Port_Max) Port_Max = n;*/

	if (down)
		_Start = now;
	else
		PressTime = (ushort)(now - _Start);

	if (HardEvent || !_IRQ)
		Press(*this, down);
	else
		Sys.SetTask(_task, true, ShakeTime);
}

void InputPort::InputTask(void* param)
{
	auto port = (InputPort*)param;
	byte v = port->_Value;
	/*if (port->_qu_read == port->_qu_write) return;

	byte v = port->_queue[port->_qu_read++];
	if (port->_qu_read >= sizeof(port->_queue))port->_qu_read = 0;
	if (port->_qu_read != port->_qu_write) Sys.SetTask(port->_task, true, 0);*/
	if (!v) return;

	//v	&= port->Mode;
	if (v & Rising)		port->Press(*port, true);
	if (v & Falling)	port->Press(*port, false);
}

void InputPort::InputNoIRQTask(void* param)
{
	auto port = (InputPort*)param;
	auto val = port->Read();

	port->OnPress(val);
}

void InputPort::OnOpen(void* param)
{
	TS("InputPort::OnOpen");

	// 如果不是硬件事件，则默认使用20ms抖动
	if (!HardEvent && ShakeTime == 0) ShakeTime = 20;
#if DEBUG
	debug_printf(" 抖动=%dms", ShakeTime);
	if (Floating)
		debug_printf(" 浮空");
	else if (Pull == UP)
		debug_printf(" 上升沿");
	else if (Pull == DOWN)
		debug_printf(" 下降沿");
	//if(Mode & Rising)	debug_printf(" 按下");
	//if(Mode & Falling)	debug_printf(" 弹起");

	bool fg = false;
#endif

	Port::OnOpen(param);
	OpenPin(param);

	// 根据倒置情况来获取初始状态，自动判断是否倒置
	bool rs = Port::Read();
	if (Invert > 1)
	{
		Invert = rs;
#if DEBUG
		fg = true;
#endif
	}

#if DEBUG
	if (Invert)
	{
		if (fg)
			debug_printf(" 自动倒置");
		else
			debug_printf(" 倒置");
	}
#endif

#if DEBUG
	debug_printf(" 初始电平=%d \r\n", rs);
#endif
}

void InputPort::OnClose()
{
	Port::OnClose();

	ClosePin();
}

// 输入轮询时间间隔。默认100ms，允许外部修改
uint InputPort_Polling = 100;

bool InputPort::UsePress()
{
	assert(_Pin != P0, "输入注册必须先设置引脚");

	_IRQ = OnRegister();

	if (!_task && !HardEvent)
	{
		// 如果硬件中断注册失败，则采用10ms定时读取
		if (_IRQ)
			_task = Sys.AddTask(InputTask, this, -1, -1, "输入事件");
		else
			_task = Sys.AddTask(InputNoIRQTask, this, InputPort_Polling, InputPort_Polling, "输入轮询");
	}

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
