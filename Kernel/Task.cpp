#include "Task.h"
#include "TTime.h"

Task::Task()
{
	Init();
}

Task::~Task()
{
	if(ID) Host->Remove(ID);
}

void Task::Init()
{
	Host		= nullptr;

	ID			= 0;
	Name		= nullptr;

	Callback	= nullptr;
	Param		= nullptr;

	Period		= 0;
	NextTime	= 0;

	Times		= 0;
	SleepTime	= 0;
	Cost		= 0;
	CostMs		= 0;
	MaxCost		= 0;

	Enable		= true;
	Event		= false;
	Deepth		= 0;
	MaxDeepth	= 1;
}

// 关键性代码，放到开头
INROOT bool Task::Execute(UInt64 now)
{
	TS(Name);

	auto& dp	= Deepth;
	if(dp >= MaxDeepth) return false;
	dp++;

	// 如果是事件型任务，这里禁用。任务中可以重新启用
	if(Event)
		Enable	= false;
	// 不能通过累加的方式计算下一次时间，因为可能系统时间被调整
	else
		NextTime	= now + Period;

	TimeCost tc;
	SleepTime	= 0;

	auto cur	= Host->Current;

	Host->Current = this;
	Callback(Param);
	Host->Current = cur;

	// 累加任务执行次数和时间
	Times++;
	int ct = tc.Elapsed();
	//if(ct < 0) debug_printf("cost = %d \r\n", ct);

	ct -= SleepTime;
	if(ct > MaxCost) MaxCost = ct;
	// 根据权重计算平均耗时
	Cost	= (Cost * 5 + ct * 3) >> 3;
	CostMs	= Cost / 1000;

#if DEBUG
	if(ct > 500000) debug_printf("Task::Execute 任务 %d [%d] 执行时间过长 %dus 睡眠 %dus\r\n", ID, Times, ct, SleepTime);
#endif

	// 如果只是一次性任务，在这里清理
	if(!Event && Period < 0) Host->Remove(ID);

	dp--;

	return true;
}

// 设置任务的开关状态，同时运行指定任务最近一次调度的时间，0表示马上调度
INROOT void Task::Set(bool enable, int msNextTime)
{
	Enable	= enable;

	// 可以安排最近一次执行的时间，比如0表示马上调度执行
	if(msNextTime >= 0) NextTime = Sys.Ms() + msNextTime;

	// 如果系统调度器处于Sleep，让它立马退出
	if(enable) Scheduler()->SkipSleep();
}

INROOT bool Task::CheckTime(UInt64 end, bool isSleep)
{
	if(Deepth >= MaxDeepth) return false;

	UInt64 now = Sys.Ms();
	if(NextTime > 0 && NextTime > now) return false;

	// 事件型任务，并且当前可用时间超过10ms，允许调度
	//if(Event && now + 10 < end) return true;

	// 并且任务的平均耗时要足够调度，才安排执行，避免上层是Sleep时超出预期时间
	if(now + (UInt64)CostMs > end) return false;

	if(!isSleep) return true;

	// 只有被调度过的任务，才会在Sleep里面被再次调度
	if(Event || Times > 0) return true;

	// 还没有经过调度的普通任务，在剩余时间超过500ms时，也给予调度机会
	// 调试WiFi产品发行版时发现，打开8266需要等待3000ms，然后看门狗没有被调度过，导致没有机会执行
	if(now + 500 < end) return true;

	return false;
}

// 全局任务调度器
TaskScheduler* Task::Scheduler()
{
	static TaskScheduler _sc("Task");

	return &_sc;
}

INROOT Task* Task::Get(int taskid)
{
	return (*Scheduler())[taskid];
}

INROOT Task& Task::Current()
{
	return *(Scheduler()->Current);
}

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

TaskScheduler::TaskScheduler(cstring name)
{
	Name 	= name;

	Running = false;
	Current	= nullptr;
	Count	= 0;
	Deepth	= 0;
	MaxDeepth	= 8;

	Times	= 0;
	Cost	= 0;
	TotalSleep	= 0;
	LastTrace	= Sys.Ms();

	_SkipSleep	= false;

	EnterSleep	= nullptr;
	ExitSleep	= nullptr;
}

// 使用外部缓冲区初始化任务列表，避免频繁的堆分配
void TaskScheduler::Set(Task* tasks, int count)
{
	for(int i=0; i<count; i++)
	{
		tasks[i].ID	= 0;
		_Tasks.Add(&tasks[i]);
	}
}

uint TaskScheduler::FindID(Action func)
{
	if (!func) return 0;
	uint id = 0;
	for (int i = 0; i < _Tasks.Count(); i++)
	{
		auto task = (Task*)_Tasks[i];
		if (task->Callback == func)
		{
			id = task->ID ;
			break;
		}
	}
	return id;
}

Task* TaskScheduler::FindTask(Action func)
{
	if (!func) return nullptr;
	Task* ret = nullptr;
	for (int i = 0; i < _Tasks.Count(); i++)
	{
		auto task = (Task*)_Tasks[i];
		if (task->Callback == func)
		{
			ret = task;
			break;
		}
	}
	return ret;
}

// 创建任务，返回任务编号。dueTime首次调度时间ms，-1表示事件型任务，period调度间隔ms，-1表示仅处理一次
uint TaskScheduler::Add(Action func, void* param, int dueTime, int period, cstring name)
{
	// 查找是否有可用空闲任务
	Task* task	= nullptr;
	for(int i=0; !task && i<_Tasks.Count(); i++)
	{
		auto ti	= (Task*)_Tasks[i];
		if(ti->ID == 0) task	= ti;
	}
	if(task)
		task->Init();
	else
		_Tasks.Add(task = new Task());

	static uint _gid = 1;

	task->Host	= this;
	task->ID	= _gid++;
	task->Name	= name;
	task->Callback	= func;
	task->Param		= param;
	task->Period	= period;

	if(dueTime < 0)
	{
		task->NextTime	= 0;
		task->Enable	= false;
		task->Event		= true;
	}
	else
		task->NextTime	= Sys.Ms() + dueTime;

	Count++;

#if DEBUG
	debug_printf("%s::Add%d %s First=%dms Period=%dms 0x%p\r\n", Name, task->ID, name, dueTime, period, func);
#endif

	return task->ID;
}

void TaskScheduler::Remove(uint taskid)
{
	if(!taskid) return;

	for(int i=0; i<_Tasks.Count(); i++)
	{
		auto task = (Task*)_Tasks[i];
		if(task->ID == taskid)
		{
			debug_printf("%s::Remove%d %s 0x%p\r\n", Name, task->ID, task->Name, task->Callback);
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
	Add(&TaskScheduler::ShowStatus, this, 10000, 30000, "任务状态");
#endif
	debug_printf("%s::准备就绪 开始循环处理%d个任务！\r\n\r\n", Name, Count);

	bool cancel	= false;
	Running = true;
	Deepth++;
	while(Running)
	{
		Execute(0xFFFFFFFF, cancel);
	}
	Deepth--;
	Running = false;
	debug_printf("%s停止调度，共有%d个任务！\r\n", Name, Count);
}

void TaskScheduler::Stop()
{
	debug_printf("%s停止！\r\n", Name);
	Running = false;
}

// 关键性代码，放到开头
// 执行一次循环。指定最大可用时间
INROOT void TaskScheduler::Execute(uint msMax, bool& cancel)
{
	TS("Task::Execute");

	UInt64 now	= Sys.Ms();
	UInt64 end	= now + msMax;
	UInt64 min	= UInt64_Max;		// 最小时间，这个时间就会有任务到来

	TimeCost tc;

	for(int i=0; i<_Tasks.Count(); i++)
	{
		// 如果外部取消，马上退出调度
		if(cancel) return;

		auto task	= (Task*)_Tasks[i];
		if(!task || task->ID == 0 || !task->Enable) continue;

		if(task->CheckTime(end, msMax != 0xFFFFFFFF))
		{
			if(task->Execute(now)) Times++;

			// 为了确保至少被有效调度一次，需要在被调度任务内判断
			// 如果已经超出最大可用时间，则退出
			if(!msMax || Sys.Ms() > end) return;
		}
		// 注意Execute内部可能已经释放了任务
		if(task->ID && task->Enable)
		{
			// 如果事件型任务还需要执行，那么就不要做任何等待
			if(task->Event)
				min	= 0;
			else if(task->NextTime < min)
				min	= task->NextTime;
		}
	}

	Cost	+= tc.Elapsed();

	// 有可能这一次轮询是有限时间
	if(min > end) min	= end;
	// 如果有最小时间，睡一会吧
	now = Sys.Ms();	// 当前时间
	if(/*msMax == 0xFFFFFFFF &&*/ !_SkipSleep && min != UInt64_Max && min > now)
	{
		min	-= now;
		Sleeping	= true;
		// 通知外部，需要睡眠若干毫秒
		if(EnterSleep)
			EnterSleep((int)min);
		else
			Time.Sleep((int)min, &Sleeping);
		Sleeping	= false;

		// 累加睡眠时间
		Int64 ms	= (Int64)Sys.Ms() - (Int64)now;
		TotalSleep	+= ms;
	}
	_SkipSleep	= false;
}

INROOT uint TaskScheduler::ExecuteForWait(uint msMax, bool& cancel)
{
	auto& dp	= Deepth;
	if(dp >= MaxDeepth) return 0;
	dp++;

	// 记录当前正在执行任务
	auto task = Current;

	auto start	= Sys.Ms();
	auto end	= start + msMax;
	auto ms		= (int)msMax;

	TimeCost tc;
	// 如果休眠时间足够长，允许多次调度其它任务
	while(ms > 0 && !cancel)
	{
		Execute(ms, cancel);

		// 统计这次调度的时间
		ms	= (int)(end - Sys.Ms());
	}
	Current	= task;

	int cost	= (int)(Sys.Ms() - start);
	if(task) task->SleepTime	+= tc.Elapsed();

	dp--;

	return cost;
}

// 跳过最近一次睡眠，马上开始下一轮循环
INROOT void TaskScheduler::SkipSleep()
{
	_SkipSleep	= true;
	Sleeping	= false;

	// 通知外部，要求退出睡眠，恢复调度
	if(ExitSleep) ExitSleep();
}

#include "Heap.h"

// 显示状态
void TaskScheduler::ShowStatus()
{
#if DEBUG
	auto now	= Sys.Ms();

	// 分段统计负载均值
	auto ts	= Times;
	auto ct	= Cost;
	auto p	= 10000 - (int)(TotalSleep * 10000 / (now - LastTrace));

	Times	= 0;
	Cost	= 0;
	TotalSleep	= 0;
	LastTrace	= now;

	debug_printf("Task::ShowStatus [%d]", ts);
	debug_printf(" 负载 %d.%d%%", p/100, p%100);
	debug_printf(" 平均 %dus 当前 ", ts ? ct/ts : 0);
	DateTime::Now().Show();
	debug_printf(" 启动 ");
	TimeSpan(now).Show(false);

	auto hp	= Heap::Current;
	debug_printf(" 堆 %d/%d\r\n", hp->Used(), hp->Count());

	// 计算任务执行的平均毫秒数，用于中途调度其它任务，避免一个任务执行时间过长而堵塞其它任务
	int ms = ct / 1000;
	if(ms > 10) ms = 10;

	for(int i=0; i<_Tasks.Count(); i++)
	{
		auto task = (Task*)_Tasks[i];
		if(task && task->ID)
		{
			task->ShowStatus();
			Sys.Sleep(ms);
		}
	}
#endif
}

Task* TaskScheduler::operator[](int taskid)
{
	for(int i=0; i<_Tasks.Count(); i++)
	{
		auto task = (Task*)_Tasks[i];
		if(task->ID == (uint)taskid) return task;
	}

	return nullptr;
}
