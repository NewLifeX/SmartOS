#ifndef __ACZero_H__
#define __ACZero_H__

#include "Kernel\TTime.h"
#include "Device\Port.h"

// 交流过零检测
class ACZero
{
public:
	InputPort	Port;	// 交流过零检测引脚
	int		Period;		// 周期us
	int		Width;		// 零点信号宽度ms
	uint	Count;		// 累计次数
	TimeCost	Last;		// 上一次零点

	ACZero();
	~ACZero();

	// 设置引脚，0下拉1上拉2自动检测
	void Set(Pin pin, byte invert = 0);
	bool Open();
	void Close();

	// 等待下一次零点，需要考虑继电器动作延迟
	bool Wait(int usDelay = 0) const;

private:
	void OnHandler(InputPort& port, bool down);
};

#endif
