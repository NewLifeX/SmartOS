
#include "PulsePort.h"

PulsePort::PulsePort() {}

PulsePort::PulsePort(Pin pin)
{
	if (pin == P0)return;
	_Port = new InputPort(pin);
	needFree = true;
}

bool PulsePort::Set(InputPort * port, uint shktime)
{
	if (port == nullptr)return false;

	_Port = port;
	needFree = false;

	ShakeTime = shktime;
	return true;
}

bool PulsePort::Set(Pin pin, uint shktime)
{
	if (pin == P0)return false;

	_Port = new InputPort(pin);
	needFree = true;
	ShakeTime = shktime;
	return false;
}

void PulsePort::Open()
{
	if (_Port == nullptr)return;
	if (Handler == nullptr)return;

	if (Opened)return;
	_Port->HardEvent = true;
	if (!_Port->Register([](InputPort* port, bool down, void* param) {((PulsePort*)param)->OnHandler(port, down); }, this))
	{
		// 注册失败就返回 不要再往下了  没意义
		return;
	}
	_Port->Open();

	_task = Sys.AddTask(
		[](void* param)
	{
		auto port = (PulsePort*)param;	
		Sys.SetTask(port->_task, false);
		port->Handler(port, port->Value, port->Param);
	},
		this, ShakeTime, ShakeTime, "PulsePort触发任务");

	Opened = true;
}

void PulsePort::Close()
{
	_Port->Close();
	Opened = false;
	if (needFree)
		delete _Port;
}

void PulsePort::Register(PulsePortHandler handler, void* param)
{
	if (handler)
	{
		Handler = handler;
		Param = param;
	}
}

void PulsePort::OnHandler(InputPort* port, bool down)
{
	// 取UTC时间的MS值
	UInt64 now = Sys.Seconds() * 1000 + Sys.Ms() - Time.Milliseconds;

	if (!Value)
	{
		//第一个信号到达;
		LastTriTime = now;
		Value = true;
		if (_task)Sys.SetTask(_task, true, -1);
		return;
	}
	auto time = now - LastTriTime;
	// 计算当前信号与上一个信号的时间差，对比抖动时间，看是否合格的信号
	if (ShakeTime <= now - LastTriTime)
	{
		// 赋值脉冲间隔，给外部使用
		Intervals = time;
		// 完成一次符合标准脉冲信号,触发事件任务
		LastTriTime = now;		
		if (_task)Sys.SetTask(_task, true, -1);
		return;
	}
}
