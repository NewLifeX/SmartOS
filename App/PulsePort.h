
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
    typedef void (*PulsePortHandler)(PulsePort* port, bool hasPulse, void* param);
private:
	uint		ShkPulse= 0;		// ShakeTime/Intervals/2 即 去抖脉冲个数 提前计算降低中断代价
	uint		ShkCnt 	= 0;		// ShakeTime 期间脉冲计数
	uint		_task 	= 0;		// 任务
	
	InputPort *	_Port 	= nullptr;	// 输入引脚
	bool	needFree	= false;	// 是否需要释放对象
	UInt64 	ShkTmeStar;		
	UInt64  LastTriTime;			// 上一个信号触发时间
	bool	Value		= false;	// 是否形成脉冲,至少有两个信号才能形成脉冲波形
									
	bool LockTime = false;			// 锁住时间，不要乱动

    PulsePortHandler	Handler	= nullptr;
	void*				Param	= nullptr;
public:
	PulsePort();
	PulsePort(Pin pin);				// 默认浮空输入
	bool Set(InputPort * port = nullptr, uint shktime = 0);
	bool Set(Pin pin, uint shktime = 0);
	
	// 如果是us  就没有程序处理的必要了  速度太快 会严重占用CPU时间的！！！
	// 所以是不接受 us脉冲间隔的脉冲IO输入设备

	// 脉冲间隔 单位 ms 
	uint Intervals	= 0;			
	// 去抖时间
	uint ShakeTime	= 0;			
	
	void Open();
	void Close();
	// 内部中断函数
	void OnHandler(InputPort* port,bool down);
	// 注册回调函数
	void Register(PulsePortHandler handler = NULL, void* param = NULL);
	
	bool		Opened 	= false;	// 是否是打开的	
};

#endif 
