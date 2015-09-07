#include "BlinkPort.h"
#include "Task.h"

static void BlinkTask(void* param);

BlinkPort::BlinkPort()
{
	_tid	= 0;

	ArrayZero(Ports);
	Count	= 0;

	First	= true;
	Times	= 10;
	Interval1	= 100000;
	Interval2	= 300000;
	Index		= 0;
}

BlinkPort::~BlinkPort()
{
	if(_tid) Sys.RemoveTask(_tid);
}

void BlinkPort::Add(OutputPort* port)
{
	Ports[Count++] = port;
}

void BlinkPort::Start()
{
	if(!Count) return;

	Current	= First;
	Index	= 0;

	for(int i=0; i<Count; i++)
	{
		Ports[i]->Open();
	}

	if(!_tid) _tid = Sys.AddTask(BlinkTask, this, -1, -1, "闪烁端口");

	// 开启一次任务
	Sys.SetTask(_tid, true);
}

void BlinkPort::Blink()
{
	bool f = Current;
	int t = Interval1;
	if(Index & 0x01) t = Interval2;

	//debug_printf("翻转到状态 %d Index=%d Next=%d\r\n", Current, Index, t);

	for(int i=0; i<Count; i++)
	{
		Ports[i]->Write(f);
	}

	// 如果闪烁次数不够，则等下一次
	Index++;
	if(Index < Times)
	{
		Current = !Current;

		// 切换任务执行时间
		Sys.SetTask(_tid, true, t);
	}
}

void BlinkTask(void* param)
{
	BlinkPort* bp = (BlinkPort*)param;
	bp->Blink();
}
