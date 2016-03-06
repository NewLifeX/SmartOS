#include "AT24CXX.h"

#define AT24C01		127
#define AT24C02		255
#define AT24C04		511
#define AT24C08		1023
#define AT24C16		2047
#define AT24C32		4095
#define AT24C64	   8191    //256 Pages of  32 bytes
#define AT24C128	   16383   //256 Pages of  64bytes
#define AT24C256	   32767   //512 Pages of  64bytes
#define AT24C512	   65535   //512 Pages of 128bytes

#define EE_PAGES        512     //存储器页数目
#define EE_BYTES        128     //每页字节数目
#define EE_TYPE         AT24C512//存储器型号
#define EE_TDEA         10      //页延时  ms

AT24CXX::AT24CXX()
{
	IIC		= NULL;

	// 7位地址，到了I2C那里，需要左移1位
	Address	= 0x50;
}

AT24CXX::~AT24CXX()
{
	delete IIC;
	IIC = NULL;
}

void AT24CXX::Init()
{
	debug_printf("\r\nAT24CXX::Init Address=0x%02X \r\n", Address);

	IIC->SubWidth	= 1;
	IIC->Address	= Address << 1;
}

bool AT24CXX::Write(ushort addr, byte data)
{
	if(!IIC) return false;

	IIC->Address = (Address + (addr >> 8)) << 1;

	return IIC->Write(addr & 0xFF, data);
}

byte AT24CXX::Read(ushort addr)
{
	if(!IIC) return 0;

	IIC->Address = (Address + (addr >> 8)) << 1;

	return IIC->Read(addr & 0xFF);
}

bool AT24CXX::Write(uint addr, const Buffer& bs) const
{
	if(!IIC) return false;
	
	IIC->Address = Address << 1;

	return IIC->Write((ushort)addr, bs);
}

bool AT24CXX::Read(uint addr, Buffer& bs) const
{
	if(!IIC) return false;

	IIC->Address = Address << 1;
	
	uint len = IIC->Read((ushort)addr, bs);
	if(len == 0)return false;
	if(len != bs.Length()) bs.SetLength(len);

	return true;
}
