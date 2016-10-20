
#ifndef __Pulse_Port_H__
#define __Pulse_Port_H__

#include "Kernel\Sys.h"
//#include "..\Kernel\TTime.h"
//#include "Port.h"

class InputPort;

// 输入类型为脉冲的IO驱动
class PulsePort
{
public:
	InputPort*	Port;	// 输入引脚

	uint Min;		// 最小时间间隔 单位 ms
	uint Max;		// 最大时间间隔 单位 ms

	UInt64  Last;	// 上一个信号触发时间
	uint  Time;		// 触发时间
	uint  Times;	// 触发次数
	bool	Opened;	// 是否打开

	Delegate<PulsePort&> Press;
	
	PulsePort();

	void Open();
	void Close();

private:
	// 内部中断函数
	void OnPress(bool down);
};

#endif
