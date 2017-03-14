#ifndef __ACZero_H__
#define __ACZero_H__

#include "Device\Port.h"

// 交流过零检测
class ACZero
{
public:
	InputPort	Port;	// 交流过零检测引脚
	int Time;			// 10ms为基数的零点延迟时间ms
	//int AdjustTime;		// 过零检测时间补偿。默认2ms

	ACZero();
	~ACZero();

	void Set(Pin pin);
	bool Open();
	void Close();

	bool Check();

private:
	uint	_taskid;
};

#endif
