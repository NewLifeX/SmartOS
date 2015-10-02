#ifndef _AT24CXX_H_
#define _AT24CXX_H_

#include "I2C.h"

// 光强传感器
class AT24CXX
{
public:
    I2C* IIC;		// I2C通信口
	byte Address;	// 设备地址

    AT24CXX();
    virtual ~AT24CXX();

	void Init();
	bool Write(ushort addr, byte data);
	byte Read(ushort addr);

	bool Write(ushort addr, const ByteArray& bs);
	uint Read(ushort addr, ByteArray& bs);
};

#endif
