#ifndef _AT24CXX_H_
#define _AT24CXX_H_

#include "I2C.h"
#include "FlashFace.h"

// 光强传感器
class AT24CXX : public FlashFace
{
public:
    I2C* IIC;		// I2C通信口
	byte Address;	// 设备地址

    AT24CXX();
    virtual ~AT24CXX();

	void Init();
	bool Write(ushort addr, byte data);
	byte Read(ushort addr);

	virtual bool Write(uint addr, const ByteArray& bs, bool readModifyWrite = true);
	virtual bool Read(uint addr, ByteArray& bs);
};

#endif
