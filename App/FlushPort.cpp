#include "FlushPort.h"
#include "Task.h"

FlushPort::FlushPort()
{
	_tid	= 0;
	Port	= nullptr;
	Fast	= 50;
	Slow	= 1000;
	Count	= 0;
}

FlushPort::~FlushPort()
{
	Sys.RemoveTask(_tid);
}

static void FlushPortTask(void* param)
{
	FlushPort* bp = (FlushPort*)param;
	bp->Flush();
}

void FlushPort::Start(int ms)
{
	if(!Port || !Port->Open()) return;

	if(!_tid)
	{
		int speed = Slow;
		if(speed <= 0) speed = Fast;
		_tid = Sys.AddTask(FlushPortTask, this, speed, speed, "闪烁端口");
	}

	// 设置任务时间
	if(ms > 0)
	{
		Count = ms / Fast;
		if(Count < 2) Count = 2;
	}
	else
		Count = -1;

	// 设置任务间隔后，马上启动任务
	Sys.SetTaskPeriod(_tid, Fast);
	Sys.SetTask(_tid, Count > 0, 0);
}

void FlushPort::Stop()
{
	if(Slow)
		Sys.SetTaskPeriod(_tid, Slow);
	else
		Sys.SetTask(_tid, false, 0);
}

void FlushPort::Flush()
{
	*Port = !*Port;

	if(Count > 0 && --Count == 0)
	{
		// 切换任务执行时间
		if(Slow)
			Sys.SetTaskPeriod(_tid, Slow);
		else
			Sys.SetTask(_tid, false, 0);
	}
}

int FlushPort::Write(byte* data)
{
	Start(*(int*)data);

	return 1;
}
