#ifndef __Task_H__
#define __Task_H__

#include "Sys.h"
#include "List.h"

class TaskScheduler;

// 全局任务调度器
extern TaskScheduler Scheduler;

// 任务
class Task
{
private:
	TaskScheduler* _Scheduler;

	friend class TaskScheduler;

	Task(TaskScheduler* scheduler);

public:
	uint	ID;			// 编号
	string	Name;		// 名称

	Action	Callback;	// 回调
	void*	Param;		// 参数

	long	Period;		// 周期us
	long	NextTime;	// 下一次执行时间

	int		Times;		// 执行次数
	int		CpuTime;	// 总耗费时间
	int		SleepTime;	// 当前睡眠时间
	int		Cost;		// 平均执行时间
	int		MaxCost;	// 最大执行时间

	bool	Enable;		// 是否启用
	byte	Deepth;		// 当前深度
	byte	MaxDeepth;	// 最大深度。默认1层，不允许重入
	byte	Reversed[3];// 保留，避免对齐问题

	~Task();

	// 执行任务。返回是否正常执行。
	bool Execute(ulong now);
	void ShowStatus();	// 显示状态
};

// 任务调度器
class TaskScheduler
{
private:
	FixedArray<Task, 16> _Tasks;
	uint _gid;	// 总编号

	friend class Task;

public:
	string	Name;		// 系统名称
	int		Count;		// 任务个数
	Task*	Current;	// 正在执行的任务
	bool	Running;	// 是否正在运行
	bool	Sleeping;	// 如果当前处于Sleep状态，马上停止并退出
	byte	Reversed[2];// 保留，避免对齐问题

	int		Cost;		// 平均执行时间
	int		MaxCost;	// 最大执行时间

	TaskScheduler(string name = NULL);
	~TaskScheduler();

	// 创建任务，返回任务编号。dueTime首次调度时间us，-1表示事件型任务，period调度间隔us，-1表示仅处理一次
	uint Add(Action func, void* param, long dueTime = 0, long period = 0, string name = NULL);
	void Remove(uint taskid);

	void Start();
	void Stop();
	// 执行一次循环。指定最大可用时间
	void Execute(uint usMax);

	static void ShowStatus(void* param);	// 显示状态

    Task* operator[](int taskid);
};

#endif
