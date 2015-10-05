#ifndef __Task_H__
#define __Task_H__

#include "Sys.h"
#include "List.h"

class TaskScheduler;

// 任务
class Task
{
private:
	friend class TaskScheduler;

public:
	TaskScheduler* Host;

	uint	ID;			// 编号
	string	Name;		// 名称

	Action	Callback;	// 回调
	void*	Param;		// 参数

	int		Period;		// 周期ms
	Int64	NextTime;	// 下一次执行时间ms

	int		Times;		// 执行次数
	int		CpuTime;	// 总耗费时间
	int		SleepTime;	// 当前睡眠时间
	int		Cost;		// 平均执行时间
	int		MaxCost;	// 最大执行时间

	bool	Enable;		// 是否启用
	bool	Event;		// 是否只执行一次后暂停的事件型任务
	byte	Deepth;		// 当前深度
	byte	MaxDeepth;	// 最大深度。默认1层，不允许重入

	Task();
	~Task();

	// 执行任务。返回是否正常执行。
	bool Execute(ulong now);
	void ShowStatus();	// 显示状态

	// 全局任务调度器
	static TaskScheduler* Scheduler();
	static Task* Get(int taskid);
};

// 任务调度器
class TaskScheduler
{
private:
	IArray<Task>*	_Tasks;
	uint _gid;	// 总编号

	friend class Task;

public:
	string	Name;		// 系统名称
	int		Count;		// 任务个数
	Task*	Current;	// 正在执行的任务
	bool	Running;	// 是否正在运行
	bool	Sleeping;	// 如果当前处于Sleep状态，马上停止并退出

	int		Cost;		// 平均执行时间
	int		MaxCost;	// 最大执行时间

	TaskScheduler(string name = NULL);
	~TaskScheduler();

	void Set(IArray<Task>* tasks);

	// 创建任务，返回任务编号。dueTime首次调度时间ms，-1表示事件型任务，period调度间隔ms，-1表示仅处理一次
	uint Add(Action func, void* param, int dueTime = 0, int period = 0, string name = NULL);
	void Remove(uint taskid);

	void Start();
	void Stop();
	// 执行一次循环。指定最大可用时间
	void Execute(uint msMax);

	static void ShowStatus(void* param);	// 显示状态

    Task* operator[](int taskid);
};

#endif
