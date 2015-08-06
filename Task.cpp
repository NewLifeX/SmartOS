#include "Task.h"
#include "Time.h"

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
	MaxCost		= 0;
	Enable		= true;
	Deepth		= 0;
	MaxDeepth	= 1;
}

Task::~Task()
{
	if(ID) _Scheduler->Remove(ID);
}

bool Task::Execute(ulong now)
{
	if(Deepth >= MaxDeepth) return false;
	Deepth++;

	// 不能通过累加的方式计算下一次时间，因为可能系统时间被调整
	if(NextTime >= 0)
		NextTime = now + Period;
	// 如果是事件型任务，这里禁用。任务中可以重新启用
	else
		Enable = false;

	TimeCost tc;
	SleepTime = 0;

	Task* cur = _Scheduler->Current;
	_Scheduler->Current = this;
	Callback(Param);
	_Scheduler->Current = cur;

	// 累加任务执行次数和时间
	Times++;
	int cost = tc.Elapsed();
	if(cost < 0) debug_printf("cost = %d \r\n", cost);
	if(cost < 0) cost = -cost;
	//if(cost > 0)
	{
		cost -= SleepTime;
		if(cost > MaxCost) MaxCost = cost;
		CpuTime += cost;
		Cost = CpuTime / Times;
	}

#if DEBUG
	if(cost > 500000) debug_printf("Task::Execute 任务 %d [%d] 执行时间过长 %dus 睡眠 %dus\r\n", ID, Times, cost, SleepTime);
#endif

	// 如果只是一次性任务，在这里清理
	if(NextTime >= 0 && Period < 0) _Scheduler->Remove(ID);

	Deepth--;

	return true;
}

// 显示状态
void Task::ShowStatus()
{
	debug_printf("Task::%s \t%d [%d] \t平均 %dus ", Name, ID, Times, Cost);
	if(Cost < 1000) debug_printf("\t");

	debug_printf("\t最大 %dus ", MaxCost);
	if(MaxCost < 1000) debug_printf("\t");

	debug_printf("\t周期 ");
	if(Period >= 1000000)
		debug_printf("%ds", (int)(Period / 1000000));
	else if(Period >= 1000)
		debug_printf("%dms", (int)(Period / 1000));
	else
		debug_printf("%dus", (int)Period);
	if(!Enable) debug_printf(" 禁用");
	debug_printf("\r\n");
}

TaskScheduler::TaskScheduler(string name)
{
	Name = name;

	_gid = 1;

	Running = false;
	Current	= NULL;
	Count	= 0;

	Cost	= 0;
	MaxCost	= 0;
}

TaskScheduler::~TaskScheduler()
{
	Current = NULL;
	_Tasks.DeleteAll().Clear();
}

// 创建任务，返回任务编号。dueTime首次调度时间us，-1表示事件型任务，period调度间隔us，-1表示仅处理一次
uint TaskScheduler::Add(Action func, void* param, Int64 dueTime, Int64 period, string name)
{
	Task* task	= new Task(this);
	task->ID	= _gid++;
	task->Name	= name;
	task->Callback	= func;
	task->Param		= param;
	task->Period	= period;

	if(dueTime < 0)
	{
		task->NextTime	= dueTime;
		task->Enable	= false;
	}
	else
		task->NextTime	= Time.Current() + dueTime;

	Count++;
	_Tasks.Add(task);

#if DEBUG
	// 输出长整型%ld，无符号长整型%llu
	//debug_printf("%s添加任务%d 0x%08x FirstTime=%lluus Period=%ldus\r\n", Name, task->ID, func, dueTime, period);
	if(period >= 1000)
	{
		int dt = dueTime / 1000;
		int  pd = period > 0 ? period / 1000 : period;
		debug_printf("%s::添加%d %s 0x%08x FirstTime=%dms Period=%dms\r\n", Name, task->ID, name, func, dt, pd);
	}
	else
		debug_printf("%s::添加%d %s 0x%08x FirstTime=%dus Period=%dus\r\n", Name, task->ID, name, func, (int)dueTime, (int)period);
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
	ulong now = Time.Current();
	ulong end = now + usMax;
	now -= Sys.StartTime;	// 当前时间。减去系统启动时间，避免修改系统时间后导致调度停摆
	ulong min = UInt64_Max;		// 最小时间，这个时间就会有任务到来

	TimeCost tc;

	int i = -1;
	while(_Tasks.MoveNext(i))
	{
		Task* task = _Tasks[i];
		if(!task || !task->Enable) continue;

		if((task->NextTime <= now || task->NextTime < 0)
		// 并且任务的平均耗时要足够调度，才安排执行，避免上层是Sleep时超出预期时间
		&& Time.Current() + task->Cost <= end)
		{
			task->Execute(now);

			// 为了确保至少被有效调度一次，需要在被调度任务内判断
			// 如果已经超出最大可用时间，则退出
			if(!usMax || Time.Current() > end) return;
		}
		// 注意Execute内部可能已经释放了任务
		if(task->ID && task->Enable)
		{
			// 如果事件型任务还需要执行，那么就不要做任何等待
			if(task->NextTime < 0)
				min = 0;
			else if((ulong)task->NextTime < min)
				min = (ulong)task->NextTime;
		}
	}

	int cost = tc.Elapsed();
	if(Cost > 0)
		Cost = (Cost + cost) / 2;
	else
		Cost = cost;
	if(cost > MaxCost) MaxCost = cost;

	// 如果有最小时间，睡一会吧
	now = Time.Current();	// 当前时间
	if(min != UInt64_Max && min > now)
	{
		min -= now;
		// 睡眠时间不能过长，否则可能无法喂狗
		//if(min > 1000) min = 1000;
		Sleeping = true;
		Time.Sleep(min, &Sleeping);
		Sleeping = false;
	}
}

// 显示状态
void TaskScheduler::ShowStatus(void* param)
{
	TaskScheduler* ts = (TaskScheduler*)param;

	debug_printf("Task::ShowStatus 平均 %dus 最大 %dus 系统启动 %s \r\n", ts->Cost, ts->MaxCost, Time.Now().ToString());

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
