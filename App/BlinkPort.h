#ifndef __BlinkPort_H__
#define __BlinkPort_H__

#include "Sys.h"
#include "Port.h"

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
	int		Interval1;	// 间隔1。第一次改变状态后暂停时间，默认100000us
	int		Interval2;	// 间隔2。第二次改变状态后暂停时间，默认300000us

	bool 	Current;	// 当前值
	int		Index;		// 当前闪烁次数
	
	BlinkPort();
	~BlinkPort();

	void Add(OutputPort* port);
	void Start();
	void Blink();
};

#endif
