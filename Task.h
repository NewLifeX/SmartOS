#ifndef __Task_H__
#define __Task_H__

#include "Sys.h"
#include "List.h"

class TaskScheduler;

// 任务
class Task
{
private:
	TaskScheduler* _Scheduler;

	friend class TaskScheduler;

	Task(TaskScheduler* scheduler);
	~Task();

public:
	uint	ID;			// 编号
	Action	Callback;	// 回调
	void*	Param;		// 参数
	long	Period;		// 周期us
	ulong	NextTime;	// 下一次执行时间
};

// 任务调度器
class TaskScheduler
{
private:
	LinkedList<Task*> _List;
	uint _gid;	// 总编号

public:
	bool Running;
	int Count;

	TaskScheduler();
	~TaskScheduler();
	
	// 创建任务，返回任务编号。dueTime首次调度时间us，period调度间隔us，-1表示仅处理一次
	uint Add(Action func, void* param, ulong dueTime = 0, long period = 0);
	void Remove(uint taskid);

	void Start();
	void Stop();
};

#endif
