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

TaskScheduler::TaskScheduler()
{
	_gid = 1;

	Running = false;
	Count = 0;
}

TaskScheduler::~TaskScheduler()
{
	/*IEnumerator<Task*>* et = _List.GetEnumerator();
	while(et->MoveNext())
	{
		Task* cur = et->Current();
	}
	delete et;*/
	foreach(Task*, task, _List)
	{
		delete task;
	}
	foreach_end();
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
	_List.Add(task);

	// 输出长整型%ld，无符号长整型%llu
	debug_printf("添加任务%d 0x%08x FirstTime=%lluus Period=%ldus\r\n", task->ID, func, dueTime, period);

	return task->ID;
}

void TaskScheduler::Remove(uint taskid)
{
	foreach(Task*, task, _List)
	{
		if(task->ID == taskid)
		{
			debug_printf("删除任务%d 0x%08x\r\n", task->ID, task->Callback);
			// 首先清零ID，避免delete的时候再次删除
			task->ID = 0;
			delete task;
			break;
		}
	}
	foreach_end();
}

void TaskScheduler::Start()
{

}

void TaskScheduler::Stop()
{
}
