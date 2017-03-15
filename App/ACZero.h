#ifndef __ACZero_H__
#define __ACZero_H__

#include "Device\Port.h"

// 交流过零检测
class ACZero
{
public:
	InputPort	Port;	// 交流过零检测引脚
	int		Period;		// 周期us
	uint	Count;		// 累计次数
	UInt64	Last;		// 最后一次零点

	ACZero();
	~ACZero();

	void Set(Pin pin);
	bool Open();
	void Close();

	// 等待下一次零点，需要考虑继电器动作延迟
	bool Wait(int usDelay = 0) const;

private:
	void OnHandler(InputPort& port, bool down);
};

#endif
