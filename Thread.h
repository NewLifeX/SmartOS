#ifndef __Thread_H__
#define __Thread_H__

#include "Sys.h"

class Thread;

// 线程
class Thread
{
private:
	ushort ID;	// 编号

public:
	uint* StackTop;	// 栈顶
	uint* Stack;	// 栈底
	uint StackSize;	// 栈大小

	byte Status;	// 状态
	byte Priority;	// 优先级

	Thread* Next;
	Thread* Prev;

	Thread(Action callback, void* state = NULL, uint stackSize = 0x100);
	virtual ~Thread();

	void Start();
	void Suspend();
	void Resume();

	// 静态管理
private:
	static Thread* Head;
	static Thread* New;	// 将要被切换到的新线程

	static void PendSV_Handler(void* param);

public:
	static Thread* Current;	// 正在执行的线程

	static void Init();
	static void Schedule();
};

#endif
