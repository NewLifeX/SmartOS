#include "FlushPort.h"
#include "Task.h"

FlushPort::FlushPort()
{
	_tid	= 0;
	Port	= NULL;
	Fast	= 50000;
	Slow	= 1000000;
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

void FlushPort::Start(int speed)
{
	if(!Port || !Port->Open()) return;

	if(!_tid) _tid = Sys.AddTask(FlushPortTask, this, Slow, Slow, "闪烁端口");

	// 设置任务时间
	// Count = ms * 1000 / Fast;
	// if(Count < 2) Count = 2;
	if(speed == 0)
		Sys.SetTaskPeriod(_tid, Slow);
	else
		Sys.SetTaskPeriod(_tid, Fast);
}

void FlushPort::Stop()
{
	Sys.SetTaskPeriod(_tid, false);
}

void FlushPort::Flush()
{
	*Port = !*Port;

	// if(Count > 0 && --Count == 0)
	// {
	// 	// 切换任务执行时间
	// 	Sys.SetTaskPeriod(_tid, Slow);
	// }
}

int FlushPort::Write(byte* data)
{
	Start(*(int*)data);

	return 1;
}
