#include "Task.h"
#include "WaitHandle.h"

WaitHandle::WaitHandle()
{
	Result		= false;
}

bool WaitHandle::WaitOne(int ms)
{
	auto host	= Task::Scheduler();
	host->Execute(ms, Result);
	
	return Result;
}

void WaitHandle::Set()
{
	Result	= true;
}
