
#ifndef __Pulse_Port_H__
#define __Pulse_Port_H__

#include "..\Kernel\Sys.h"
#include "..\Kernel\TTime.h"
#include "Port.h"


// 输入类型为脉冲的IO驱动
class PulsePort
{
public:
	// PulsePort 事件函数形式
	typedef void(*PulsePortHandler)(PulsePort* port, void* param);
private:

	void	Int();
	uint		_task = 0;		// 任务
	InputPort *	_Port = nullptr;	// 输入引脚
	bool	needFree = false;	// 是否需要释放对象								
	PulsePortHandler	Handler = nullptr;
	void*				Param = nullptr;
public:
	PulsePort();
	PulsePort(Pin pin);				// 默认浮空输入
	bool Set(InputPort * port, uint minIntervals = 0, uint maxIntervals = 0);
	bool Set(Pin pin, uint minIntervals = 0, uint maxIntervals = 0);


	uint MinIntervals = 0;		// 最小时间间隔 单位 ms 
	uint MaxIntervals = 0;		// 最大时间间隔 单位 ms 


	UInt64  LastTriTime;	// 上一个信号触发时间
	UInt64  TriTime;		//触发时间
	UInt64  TriCount;		//触发次数

	void Open();
	void Close();
	// 内部中断函数
	void OnHandler(InputPort* port, bool down);
	// 注册回调函数
	void Register(PulsePortHandler handler = NULL, void* param = NULL);

	bool		Opened = false;	// 是否是打开的	
};

#endif 
