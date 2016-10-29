#ifndef _DHT11_H_
#define _DHT11_H_

#include "Device\Port.h"
#include "Device\Power.h"

// 温湿度传感器
class DHT11
{
public:
	OutputPort DA;	// 数据总线

    DHT11(Pin da);
    ~DHT11();

	void Init();
	bool Read(ushort& temp, ushort& humi);

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

private:
	byte ReadByte();
};

#endif
