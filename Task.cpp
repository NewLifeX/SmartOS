#include "Task.h"
#include "Time.h"

Task::Task()
{
	Host		= nullptr;

	ID			= 0;
	Name		= nullptr;
	Times		= 0;
	CpuTime		= 0;
	SleepTime	= 0;
	Cost		= 0;
	CostMs		= 0;
	MaxCost		= 0;
	Enable		= true;
	Event		= false;
	Deepth		= 0;
	MaxDeepth	= 1;
}

Task::~Task()
{
	if(ID) Host->Remove(ID);
}

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

bool Task::Execute(UInt64 now)
{
	//TS("Task::Execute");
	TS(Name);

	if(Deepth >= MaxDeepth) return false;
	Deepth++;

	// 如果是事件型任务，这里禁用。任务中可以重新启用
	if(Event)
		Enable = false;
	// 不能通过累加的方式计算下一次时间，因为可能系统时间被调整
	else
		NextTime = now + Period;

	TimeCost tc;
	SleepTime = 0;

	auto cur = Host->Current;
	// 其实默认最大深度为1，已经禁止所有任务重入，需要重入的任务得专门设置
	/*// 事件型任务和一次性任务，禁止重入
	if(cur == this)
	{
		if(Event || Period < 0) return false;
	}*/

	Host->Current = this;
	Callback(Param);
	Host->Current = cur;

	// 累加任务执行次数和时间
	Times++;
	int ct = tc.Elapsed();
	if(ct < 0)
		debug_printf("cost = %d \r\n", ct);
	if(ct < 0) ct = -ct;
	//if(ct > 0)
	{
		ct -= SleepTime;
		if(ct > MaxCost) MaxCost = ct;
		CpuTime += ct;
		Cost = CpuTime / Times;
		CostMs = Cost / 1000;
	}

#if DEBUG
	if(ct > 500000) debug_printf("Task::Execute 任务 %d [%d] 执行时间过长 %dus 睡眠 %dus\r\n", ID, Times, ct, SleepTime);
#endif

	// 如果只是一次性任务，在这里清理
	if(!Event && Period < 0) Host->Remove(ID);

	Deepth--;

	return true;
}

// 设置任务的开关状态，同时运行指定任务最近一次调度的时间，0表示马上调度
void Task::Set(bool enable, int msNextTime)
{
	Enable = enable;

	// 可以安排最近一次执行的时间，比如0表示马上调度执行
	if(msNextTime >= 0) NextTime = Sys.Ms() + msNextTime;

	// 如果系统调度器处于Sleep，让它立马退出
	if(enable) Scheduler()->Sleeping = false;
}

#pragma arm section code

// 显示状态
void Task::ShowStatus()
{
	debug_printf("Task::%s \t%d [%d] \t平均 %dus ", Name, ID, Times, Cost);
	if(Cost < 1000) debug_printf("\t");

	debug_printf("\t最大 %dus ", MaxCost);
	if(MaxCost < 1000) debug_printf("\t");

	debug_printf("\t周期 ");
	if(Period >= 1000)
		debug_printf("%ds", Period / 1000);
	else
		debug_printf("%dms", Period);
	if(!Enable) debug_printf(" 禁用");
	debug_printf("\r\n");
}

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

// 全局任务调度器
TaskScheduler* Task::Scheduler()
{
	static TaskScheduler _sc("Sys");

	return &_sc;
}

Task* Task::Get(int taskid)
{
	return (*Scheduler())[taskid];
}

#pragma arm section code

TaskScheduler::TaskScheduler(const char* name)
{
	_Tasks.Clear();
	_Tasks.SetLength(0);
	Name 	= name;

	Running = false;
	Current	= nullptr;
	Count	= 0;

	Cost	= 0;
	MaxCost	= 0;
}

// 使用外部缓冲区初始化任务列表，避免频繁的堆分配
void TaskScheduler::Set(Task* tasks, uint count)
{
	for(int i=0; i<count; i++)
	{
		tasks[i].ID	= 0;
		_Tasks.Push(&tasks[i]);
	}
}

// 创建任务，返回任务编号。dueTime首次调度时间ms，-1表示事件型任务，period调度间隔ms，-1表示仅处理一次
uint TaskScheduler::Add(Action func, void* param, int dueTime, int period, const char* name)
{
	// 查找是否有可用空闲任务
	Task* task	= nullptr;
	for(int i=0; !task && i<_Tasks.Length(); i++)
	{
		if(!_Tasks[i])
			task	= _Tasks[i] = new Task();
		else if(_Tasks[i]->ID == 0)
			task	= _Tasks[i];
	}
	if(!task) _Tasks.Push(task = new Task());

	static uint _gid = 1;

	task->Host	= this;
	task->ID	= _gid++;
	task->Name	= name;
	task->Callback	= func;
	task->Param		= param;
	task->Period	= period;

	if(dueTime < 0)
	{
		task->NextTime	= dueTime;
		task->Enable	= false;
		task->Event		= true;
	}
	else
		task->NextTime	= Sys.Ms() + dueTime;

	Count++;

#if DEBUG
	debug_printf("%s::添加%d %s First=%dms Period=%dms 0x%08x\r\n", Name, task->ID, name, dueTime, period, func);
#endif

	return task->ID;
}

void TaskScheduler::Remove(uint taskid)
{
	if(!taskid) return;

	for(int i=0; i<_Tasks.Length(); i++)
	{
		Task* task = _Tasks[i];
		if(task->ID == taskid)
		{
			debug_printf("%s::删除%d %s 0x%08x\r\n", Name, task->ID, task->Name, task->Callback);
			// 清零ID，实现重用
			task->ID = 0;

			break;
		}
	}
}

void TaskScheduler::Start()
{
	if(Running) return;

#if DEBUG
	Add(ShowStatus, this, 10000, 30000, "任务状态");
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

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

// 执行一次循环。指定最大可用时间
void TaskScheduler::Execute(uint msMax)
{
	TS("Task::Execute");

	UInt64 now = Sys.Ms();
	UInt64 end = now + msMax;
	UInt64 min = UInt64_Max;		// 最小时间，这个时间就会有任务到来

	TimeCost tc;

	for(int i=0; i<_Tasks.Length(); i++)
	{
		auto task	= _Tasks[i];
		if(task->ID == 0 || !task->Enable) continue;

		if((task->NextTime <= now || task->NextTime < 0)
		// 并且任务的平均耗时要足够调度，才安排执行，避免上层是Sleep时超出预期时间
		&& Sys.Ms() + task->CostMs <= end)
		{
			task->Execute(now);

			// 为了确保至少被有效调度一次，需要在被调度任务内判断
			// 如果已经超出最大可用时间，则退出
			if(!msMax || Sys.Ms() > end) return;
		}
		// 注意Execute内部可能已经释放了任务
		if(task->ID && task->Enable)
		{
			// 如果事件型任务还需要执行，那么就不要做任何等待
			if(task->NextTime < 0)
				min = 0;
			else if((UInt64)task->NextTime < min)
				min = (UInt64)task->NextTime;
		}
	}

	int ct = tc.Elapsed();
	if(Cost > 0)
		Cost = (Cost + ct) >> 1;
	else
		Cost = ct;
	if(ct > MaxCost) MaxCost = ct;

	// 如果有最小时间，睡一会吧
	now = Sys.Ms();	// 当前时间
	if(msMax == 0xFFFFFFFF && min != UInt64_Max && min > now)
	{
		min -= now;
		//debug_printf("任务空闲休眠 %d ms \r\n", (uint)(min/1000));
		// 睡眠时间不能过长，否则可能无法喂狗
		//if(min > 1000) min = 1000;
		Sleeping = true;
		Time.Sleep(min, &Sleeping);
		Sleeping = false;
	}
}

#pragma arm section code

// 显示状态
void TaskScheduler::ShowStatus(void* param)
{
	auto host = (TaskScheduler*)param;

	debug_printf("Task::ShowStatus 平均 %dus 最大 %dus 当前 ", host->Cost, host->MaxCost);
	Time.Now().Show();
	debug_printf(" 启动 ");
	DateTime dt(Sys.Ms() / 1000);
	dt.Show(true);

	// 计算任务执行的平均毫秒数，用于中途调度其它任务，避免一个任务执行时间过长而堵塞其它任务
	int ms = host->Cost / 1000;
	if(ms > 10) ms = 10;

	for(int i=0; i<host->_Tasks.Length(); i++)
	{
		auto task = host->_Tasks[i];
		if(task->ID)
		{
			task->ShowStatus();
			Sys.Sleep(ms);
		}
	}
}

#if !defined(TINY) && defined(STM32F0)
	#pragma arm section code = "SectionForSys"
#endif

Task* TaskScheduler::operator[](int taskid)
{
	for(int i=0; i<_Tasks.Length(); i++)
	{
		Task* task = _Tasks[i];
		if(task->ID == taskid) return task;
	}

	return nullptr;
}

#pragma arm section code
