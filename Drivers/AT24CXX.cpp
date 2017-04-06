#include "AT24CXX.h"

/*
AT24C02的存储容量为2K bit，内容分成32页，每页8Byte，共256Byte，操作时有两种寻址方式：芯片寻址和片内子地址寻址
（1）芯片寻址：AT24C02的芯片地址为1010，其地址控制字格式为1010A2A1A0R/W。其中A2，A1，A0可编程地址选择位。
A2，A1，A0引脚接高、低电平后得到确定的三位编码，与1010形成7位编码，即为该器件的地址码。R/W为芯片读写控制位，该位为0，表示芯片进行写操作。
（2）片内子地址寻址：芯片寻址可对内部256B中的任一个进行读/写操作，其寻址范围为00~FF，共256个寻址单位。
*/

#define AT24C01		127
#define AT24C02		255
#define AT24C04		511
#define AT24C08		1023
#define AT24C16		2047
#define AT24C32		4095
#define AT24C64		8191    //256 Pages of  32 bytes
#define AT24C128	16383   //256 Pages of  64bytes
#define AT24C256	32767   //512 Pages of  64bytes
#define AT24C512	65535   //512 Pages of 128bytes

#define EE_PAGES	512     //存储器页数目
#define EE_BYTES	128     //每页字节数目
#define EE_TYPE		AT24C512//存储器型号
#define EE_TDEA		10      //页延时  ms

AT24CXX::AT24CXX()
{
	IIC = nullptr;

	// A2A1A0都接低电平时，地址为 1010000，也就是0x50
	// 7位地址，到了I2C那里，需要左移1位
	Address = 0x50;
}

AT24CXX::~AT24CXX()
{
	delete IIC;
	IIC = nullptr;
}

void AT24CXX::Init()
{
	debug_printf("\r\nAT24CXX::Init Address=0x%02X \r\n", Address);

	IIC->SubWidth = 1;
	IIC->Address = Address << 1;
}

bool AT24CXX::Write(ushort addr, byte data)
{
	if (!IIC) return false;

	IIC->Address = (Address + (addr >> 8)) << 1;

	return IIC->Write(addr & 0xFF, data);
}

byte AT24CXX::Read(ushort addr) const
{
	if (!IIC) return 0;

	IIC->Address = (Address + (addr >> 8)) << 1;

	return IIC->Read(addr & 0xFF);
}

bool AT24CXX::Write(uint addr, const Buffer& bs) const
{
	if (!IIC) return false;

	IIC->Address = Address << 1;

	return IIC->Write((ushort)addr, bs);
}

bool AT24CXX::Read(uint addr, Buffer& bs) const
{
	if (!IIC) return false;

	IIC->Address = Address << 1;

	int len = IIC->Read((ushort)addr, bs);
	if (len == 0) return false;
	if (len != bs.Length()) bs.SetLength(len);

	return true;
}

ByteArray AT24CXX::Read(ushort addr, int count) const
{
	ByteArray bs;
	bs.SetLength(count);
	if (!Read(addr, bs)) bs.SetLength(0);

	return bs;
}

ushort AT24CXX::Read16(ushort addr) const
{
	auto bs = Read(addr, 2);
	if (bs.Length() < 2) return 0xFFFF;

	return bs.ToUInt16();
}

uint AT24CXX::Read32(ushort addr) const
{
	auto bs = Read(addr, 4);
	if (bs.Length() < 4) return 0xFFFFFFFF;

	return bs.ToUInt32();
}

bool AT24CXX::Write(ushort addr, ushort data)
{
	return Write(addr, Buffer(&data, 2));
}

bool AT24CXX::Write(ushort addr, uint data)
{
	return Write(addr, Buffer(&data, 4));
}
