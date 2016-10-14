
#include "PulsePort.h"

PulsePort::PulsePort() {}

PulsePort::PulsePort(Pin pin)
{
	if (pin == P0)return;
	_Port = new InputPort(pin);
	needFree = true;
}

bool PulsePort::Set(InputPort * port, uint intervals, uint shktime)
{
	if (port == nullptr)return false;
	if (intervals == 0)return false;
	_Port = port;
	needFree = false;
	Intervals = intervals;
	ShakeTime = shktime;
	return true;
}

bool PulsePort::Set(Pin pin, uint intervals, uint shktime)
{
	if (pin == P0)return false;
	if (intervals == 0)return false;
	_Port = new InputPort(pin);
	needFree = true;
	Intervals = intervals;
	ShakeTime = shktime;
	return false;
}

void PulsePort::Open()
{
	if (_Port == nullptr)return;
	if (Handler == nullptr)return;

	ShkPulse = ShakeTime / Intervals;

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

		// 从无到有一定是去抖的结果
		// 从有到无一定是超时的结果
		// 不是去抖的结果 肯定是从有到无
		if (port->ShkStat == false)
			port->Value = false;

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
	if (down) return;

	// 取UTC时间的MS值
	UInt64 now = Sys.Seconds() * 1000 + Sys.Ms() - Time.Milliseconds;

	if (Value)
	{
		LastTriTime = now;
		// 有连续脉冲情况下  一定是不用去抖的
		ShkStat = false;
		// 丢失脉冲，使用定时器来做  如果定时到了 说明中间都没有脉冲
		if (_task)Sys.SetTask(_task, true, ShakeTime);
	}
	else
	{
		// 丢失脉冲后 重新来脉冲需要考虑 去抖 （也许是干扰脉冲）
		if (!ShkStat)	// 断掉脉冲 && Shake去抖没开始
		{
			ShkTmeStar = now;
			ShkStat = true;
			ShkCnt = 1;
		}
		else
		{
			if (now >= ShkTmeStar + ShakeTime)
			{		// ShakeTime 已过
				//if(ShkCnt < ShkPulse)
				//{
				//	debug_printf("ShakeTime 已过\r\n");
				//	// 去抖结果为失败
				//	ShkTmeStar = now;
				//	ShkCnt = 1;
				//	return;
				//}
				LastTriTime = ShkTmeStar;
				Value = true;		// 有脉冲
				// 触发
				if (_task)Sys.SetTask(_task, true, -1);
				return;
			}
			//else	// 还在	ShakeTime 之中
			//{
			//	debug_printf("时间不够脉冲计数加1/r/n");
			//	ShkCnt++;
			//}
		}
	}
}
