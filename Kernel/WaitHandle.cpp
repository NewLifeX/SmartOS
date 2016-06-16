#include "Task.h"
#include "Time.h"

#include "WaitHandle.h"

WaitHandle::WaitHandle()
{
	Result		= false;
}

bool WaitHandle::WaitOne(int ms)
{
	auto host	= Task::Scheduler();
	// 记录当前正在执行任务
	auto task = host->Current;

	// 实际可用时间，-1表示无限等待
	if(ms == -1) ms	= 0x7FFFFFFF;

	TimeCost tc;

	auto now	= Sys.Ms();

	// 如果休眠时间足够长，允许多次调度其它任务
	while(ms > 0)
	{
		// 统计这次调度的时间
		UInt64 start	= now;

		host->Execute(ms, Result);

		now	= Sys.Ms();
		ms -= (int)(now - start);
	}
	if(task)
	{
		host->Current	= task;
		task->SleepTime	+= tc.Elapsed();
	}

	return Result;
}

void WaitHandle::Set()
{
	Result	= true;
}
