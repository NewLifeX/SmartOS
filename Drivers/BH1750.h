#ifndef _BH1750_H_
#define _BH1750_H_

#include "Device\Power.h"

class I2C;

// 光强传感器
class BH1750 : public Power
{
public:
    I2C*	IIC;		// I2C通信口
	byte	Address;	// 设备地址

    BH1750();
    virtual ~BH1750();

	void Init();
	ushort Read();

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

private:
	void Write(byte cmd);
};

#endif
