#ifndef __Pulse_Port_H__
#define __Pulse_Port_H__

#include "Kernel\Sys.h"

class InputPort;

// 脉冲端口。用于计算脉冲被遮挡的时间
class PulsePort
{
public:
	InputPort*	Port;	// 输入引脚

	uint	Min;	// 最小时间间隔 单位 ms
	uint	Max;	// 最大时间间隔 单位 ms

	UInt64 	Last;	// 上一个信号触发时间
	uint 	Time;	// 遮挡时间
	uint 	Times;	// 次数
	bool	Opened;	// 是否打开

	Delegate<PulsePort&> Press;	// 被遮挡时触发

	PulsePort();

	void Open();
	void Close();

private:
	// 内部中断函数
	void OnPress(InputPort& port, bool down);
};

#endif
