#ifndef __BlinkPort_H__
#define __BlinkPort_H__

#include "Sys.h"
#include "Device\Port.h"

// 闪烁端口
class BlinkPort
{
private:
	uint	_tid;

public:
	OutputPort*	Ports[0x10];
	int		Count;

	bool	First;		// 开始状态
	int		Times;		// 变更次数。高低算两次
	int		Interval1;	// 间隔1(单位ms)。第一次改变状态后暂停时间，默认100ms
	int		Interval2;	// 间隔2(单位ms)。第二次改变状态后暂停时间，默认300ms

	bool 	Current;	// 当前值
	int		Index;		// 当前闪烁次数

	BlinkPort();
	~BlinkPort();

	void Add(OutputPort* port);
	void Start();
	void Stop();
	void Blink();
};

#endif
