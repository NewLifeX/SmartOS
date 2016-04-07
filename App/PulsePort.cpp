
#include "PulsePort.h"

PulsePort::PulsePort(){}

PulsePort::PulsePort(Pin pin)
{
	if(pin == P0)return;
	_Port = new InputPort(pin);
}

bool PulsePort::Set(InputPort * port, uint intervals, uint shktime)
{
	if(port == nullptr)return false;
	_Port = port;
	Intervals = intervals;
	ShakeTime = shktime;
	return true;
}

void PulsePort::Open()
{
	if(_Port == nullptr)return;
	if(Handler == nullptr)return;
	
	ShkPulse = ShakeTime/Intervals;
	
	if(Opened)return;
	_Port->HardEvent = true;
	_Port->Register(
			[](InputPort* port, bool down, void* param){((PulsePort*)param)->OnHandler(port,down);},
			this);
	_Port->Open();
	
	_task = Sys.AddTask(
				[](void* param)
						{
						auto port = (PulsePort*)param;
						
						// 从无到有一定是去抖的结果
						// 从有到无一定是超时的结果
						// 不是去抖的结果 肯定是从有到无
						if(port->ShkStat== false)
							port->value = false;
						
						Sys.SetTask(port->_task,false);
						port->Handler(port,port->value,port->Param);
						},
				this, ShakeTime, ShakeTime, "PulsePort触发任务");
				
	Opened = true;
}

void PulsePort::Close()
{
	_Port->Close();
	Opened = false;
	delete _Port;
}

void PulsePort::Register(PulsePortHandler handler, void* param)
{
	if(handler)
	{
		Handler = handler;
		Param	= param;
	}
}

void PulsePort::OnHandler(InputPort* port,bool down)
{
	if(down)return;
	UInt64 now = Sys.Ms();
	
	if(value)
	{
		LastTriTime = now;
		// 有连续脉冲情况下  一定是不用去抖的
		ShkStat = false;
		// 丢失脉冲，使用定时器来做  如果定时到了 说明中间都没有脉冲
		if(_task)Sys.SetTask(_task, true, ShakeTime);
	}
	else
	{
		// 丢失脉冲后 重新来脉冲需要考虑 去抖 （也许是干扰脉冲）
		if(!ShkStat)	// 断掉脉冲 && Shake去抖没开始
		{
			ShkTmeStar = now;
			ShkStat = true;
			ShkCnt = 1;
		}
		else
		{
			if(now >= ShkTmeStar + ShakeTime)
			{		// ShakeTime 已过
				if(ShkCnt < ShkPulse)
				{	// 去抖结果为失败
					ShkTmeStar = now;
					ShkCnt = 1;
					return;
				}
				LastTriTime = ShkTmeStar;
				value = true;			// 有脉冲
				// 触发
				if(_task)Sys.SetTask(_task, true, -1);
				return;
			}
			else	// 还在	ShakeTime 之中
			{
				ShkCnt++;
			}
		}
	}
}
