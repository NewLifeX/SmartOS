#ifndef _AT24CXX_H_
#define _AT24CXX_H_

#include "Device\I2C.h"
#include "Storage\Storage.h"

// EEPROM
class AT24CXX : public CharStorage
{
public:
    I2C* IIC;		// I2C通信口
	byte Address;	// 设备地址

    AT24CXX();
    virtual ~AT24CXX();

	void Init();
	bool Write(ushort addr, byte data);
	byte Read(ushort addr) const;

	virtual bool Write(uint addr, const Buffer& bs) const;
	virtual bool Read(uint addr, Buffer& bs) const;

	ByteArray Read(ushort addr, int count) const;
	ushort Read16(ushort addr) const;
	uint Read32(ushort addr) const;

	bool Write(ushort addr, ushort data);
	bool Write(ushort addr, uint data);
};

/*
AT24C02的存储容量为2K bit，内容分成32页，每页8Byte，共256Byte，操作时有两种寻址方式：芯片寻址和片内子地址寻址
（1）芯片寻址：AT24C02的芯片地址为1010，其地址控制字格式为1010A2A1A0R/W。其中A2，A1，A0可编程地址选择位。
A2，A1，A0引脚接高、低电平后得到确定的三位编码，与1010形成7位编码，即为该器件的地址码。R/W为芯片读写控制位，该位为0，表示芯片进行写操作。
（2）片内子地址寻址：芯片寻址可对内部256B中的任一个进行读/写操作，其寻址范围为00~FF，共256个寻址单位。

A2A1A0都接低电平时，地址为 1010000，也就是0x50
*/

#endif
