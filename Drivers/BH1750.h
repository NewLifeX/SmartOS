#ifndef _BH1750_H_
#define _BH1750_H_

#include "I2C.h"

// 光强传感器
class BH1750
{
public:
    I2C* IIC;		// I2C通信口
	byte Address;	// 设备地址

    BH1750();
    virtual ~BH1750();

	void Init();
	ushort Read();

private:
	void Write(byte cmd);
};

#endif
