#include "Task.h"

// 全局任务调度器
TaskScheduler Scheduler("系统");

Task::Task(TaskScheduler* scheduler)
{
	_Scheduler = scheduler;

	Name		= NULL;
	Times		= 0;
	CpuTime		= 0;
	SleepTime	= 0;
	Cost		= 0;
	Enable		= true;
}

Task::~Task()
{
	if(ID) _Scheduler->Remove(ID);
}

// 显示状态
void Task::ShowStatus()
{
	debug_printf("Task::%s \t%d [%d] \t平均 %dus ", Name, ID, Times, Cost);
	if(Cost < 1000) debug_printf("\t");

	debug_printf("\t周期 ");
	if(Period >= 1000000)
		debug_printf("%ds", Period / 1000000);
	else if(Period >= 1000)
		debug_printf("%dms", Period / 1000);
	else
		debug_printf("%dus", Period);
	if(!Enable) debug_printf(" 禁用");
	debug_printf("\r\n");
}

TaskScheduler::TaskScheduler(string name)
{
	Name = name;

	_gid = 1;

	Running = false;
	Current	= NULL;
	Count = 0;
}

TaskScheduler::~TaskScheduler()
{
	Current = NULL;
	_Tasks.DeleteAll().Clear();
}

// 创建任务，返回任务编号。dueTime首次调度时间us，period调度间隔us，-1表示仅处理一次
uint TaskScheduler::Add(Action func, void* param, ulong dueTime, long period, string name)
{
	Task* task	= new Task(this);
	task->ID	= _gid++;
	task->Name	= name;
	task->Callback	= func;
	task->Param		= param;
	task->Period	= period;
	task->NextTime	= Time.Current() + dueTime;

	Count++;
	_Tasks.Add(task);

#if DEBUG
	// 输出长整型%ld，无符号长整型%llu
	//debug_printf("%s添加任务%d 0x%08x FirstTime=%lluus Period=%ldus\r\n", Name, task->ID, func, dueTime, period);
	if(period >= 1000)
	{
		uint dt = dueTime / 1000;
		int  pd = period > 0 ? period / 1000 : period;
		debug_printf("%s::添加%d %s 0x%08x FirstTime=%ums Period=%dms\r\n", Name, task->ID, name, func, dt, pd);
	}
	else
		debug_printf("%s::添加%d %s 0x%08x FirstTime=%uus Period=%dus\r\n", Name, task->ID, name, func, (uint)dueTime, (int)period);
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
			debug_printf("%s::删除%d %s 0x%08x\r\n", Name, task->ID, task->Name, task->Callback);
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
	//Add(ShowTime, NULL, 2000000, 2000000);
	Add(ShowStatus, this, 10000000, 30000000, "任务状态");
#endif
	debug_printf("%s::准备就绪 开始循环处理%d个任务！\r\n\r\n", Name, Count);

	Running = true;
	while(Running)
	{
		Execute(0xFFFFFFFF);
	}
	debug_printf("%s停止调度，共有%d个任务！\r\n", Name, Count);
}

void TaskScheduler::Stop()
{
	debug_printf("%s停止！\r\n", Name);
	Running = false;
}

// 执行一次循环。指定最大可用时间
void TaskScheduler::Execute(uint usMax)
{
	ulong now = Time.Current() - Sys.StartTime;	// 当前时间。减去系统启动时间，避免修改系统时间后导致调度停摆
	ulong min = UInt64_Max;		// 最小时间，这个时间就会有任务到来
	ulong end = Time.Current() + usMax;

	// 需要跳过当前正在执行任务的调度
	//Task* _cur = Current;

	int i = -1;
	while(_Tasks.MoveNext(i))
	{
		Task* task = _Tasks[i];
		//if(task && task != _cur && task->Enable && task->NextTime <= now)
		if(task && task->Enable && task->NextTime <= now)
		{
			// 不能通过累加的方式计算下一次时间，因为可能系统时间被调整
			task->NextTime = now + task->Period;
			if(task->NextTime < min) min = task->NextTime;

			TimeCost tc;
			task->SleepTime = 0;

			Current = task;
			task->Callback(task->Param);
			Current = NULL;

			// 累加任务执行次数和时间
			task->Times++;
			int cost = tc.Elapsed();
			if(cost < 0) debug_printf("cost = %d \r\n", cost);
			if(cost < 0) cost = -cost;
			//if(cost > 0)
			{
				task->CpuTime += cost - task->SleepTime;
				task->Cost = task->CpuTime / task->Times;
			}

#if DEBUG
			if(cost > 500000) debug_printf("Task::Execute 任务 %d [%d] 执行时间过长 %dus 睡眠 %dus\r\n", task->ID, task->Times, cost, task->SleepTime);
#endif

			// 如果只是一次性任务，在这里清理
			if(task->Period < 0) Remove(task->ID);
		}

		// 如果已经超出最大可用时间，则退出
		if(!usMax || Time.Current() > end) return;
	}

	// 如果有最小时间，睡一会吧
	now = Time.Current();	// 当前时间
	if(min != UInt64_Max && min > now)
	{
		min -= now;
#if DEBUG
		//debug_printf("TaskScheduler::Execute 等待下一次任务调度 %uus\r\n", (uint)min);
#endif
		//// 最大只允许睡眠1秒，避免Sys.Delay出现设计错误，同时也更人性化
		//if(min > 1000000) min = 1000000;
		//Sys.Delay(min);
		Time.Sleep(min);
	}
}

// 显示状态
void TaskScheduler::ShowStatus(void* param)
{
	TaskScheduler* ts = (TaskScheduler*)param;

	int i = -1;
	while(ts->_Tasks.MoveNext(i))
	{
		Task* task = ts->_Tasks[i];
		if(task) task->ShowStatus();
	}
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
