#include "Task.h"

/*

*/

Task::Task(TaskScheduler* scheduler)
{
	_Scheduler = scheduler;
}

Task::~Task()
{
	if(ID) _Scheduler->Remove(ID);
}

TaskScheduler::TaskScheduler(string name)
{
	Name = name;

	_gid = 1;

	Running = false;
	Count = 0;
}

TaskScheduler::~TaskScheduler()
{
	_Tasks.DeleteAll().Clear();
}

// 创建任务，返回任务编号。dueTime首次调度时间us，period调度间隔us，-1表示仅处理一次
uint TaskScheduler::Add(Action func, void* param, ulong dueTime, long period)
{
	Task* task = new Task(this);
	task->ID = _gid++;
	task->Callback = func;
	task->Param = param;
	task->Period = period;
	task->NextTime = Time.Current() + dueTime;

	Count++;
	_Tasks.Add(task);

#if DEBUG
	// 输出长整型%ld，无符号长整型%llu
	//debug_printf("%s添加任务%d 0x%08x FirstTime=%lluus Period=%ldus\r\n", Name, task->ID, func, dueTime, period);
	if(period >= 1000)
	{
		uint dt = dueTime / 1000;
		int  pd = period > 0 ? period / 1000 : period;
		debug_printf("%s::添加任务%d 0x%08x FirstTime=%ums Period=%dms\r\n", Name, task->ID, func, dt, pd);
	}
	else
		debug_printf("%s::添加任务%d 0x%08x FirstTime=%uus Period=%dus\r\n", Name, task->ID, func, (uint)dueTime, (int)period);
#endif

	return task->ID;
}

void TaskScheduler::Remove(uint taskid)
{
	int i = -1;
	while(_Tasks.MoveNext(i))
	{
		Task* task = _Tasks[i];
		if(task->ID == taskid)
		{
			_Tasks.RemoveAt(i);
			debug_printf("%s::删除任务%d 0x%08x\r\n", Name, task->ID, task->Callback);
			// 首先清零ID，避免delete的时候再次删除
			task->ID = 0;
			delete task;
			break;
		}
	}
}

void TaskScheduler::Start()
{
	if(Running) return;

#if DEBUG
	//AddTask(ShowTime, NULL, 2000000, 2000000);
#endif
	debug_printf("%s::准备就绪 开始循环处理%d个任务！\r\n\r\n", Name, Count);

	Running = true;
	while(Running)
	{
		ulong now = Time.Current() - Sys.StartTime;	// 当前时间。减去系统启动时间，避免修改系统时间后导致调度停摆
		ulong min = UInt64_Max;		// 最小时间，这个时间就会有任务到来

		int i = -1;
		while(_Tasks.MoveNext(i))
		{
			Task* task = _Tasks[i];
			if(task && task->NextTime <= now)
			{
				// 先计算下一次时间
				//task->NextTime += task->Period;
				// 不能通过累加的方式计算下一次时间，因为可能系统时间被调整
				task->NextTime = now + task->Period;
				if(task->NextTime < min) min = task->NextTime;

				task->Callback(task->Param);

				// 如果只是一次性任务，在这里清理
				if(task->Period < 0) delete task;
			}
		}

		// 如果有最小时间，睡一会吧
		now = Time.Current();	// 当前时间
		if(min != UInt64_Max && min > now) Sys.Delay(min - now);
	}
	debug_printf("%s停止调度，共有%d个任务！\r\n", Name, Count);
}

void TaskScheduler::Stop()
{
	debug_printf("%s停止！\r\n", Name);
	Running = false;
}

Task* TaskScheduler::operator[](int taskid)
{
	int i = -1;
	while(_Tasks.MoveNext(i))
	{
		Task* task = _Tasks[i];
		if(task && task->ID == taskid) return task;
	}
	
	return NULL;
}
