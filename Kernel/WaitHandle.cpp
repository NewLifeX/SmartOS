#include "Task.h"

#include "WaitHandle.h"

WaitHandle::WaitHandle()
{
	Result		= false;

	TaskID = Task::Current().ID;
	State = nullptr;
}

bool WaitHandle::WaitOne(int ms)
{
	// 实际可用时间，-1表示无限等待
	if(ms == -1) ms	= 0x7FFFFFFF;

	Task::Scheduler()->ExecuteForWait(ms, Result);

	return Result;
}

void WaitHandle::Set()
{
	Result	= true;
}
