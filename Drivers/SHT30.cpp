#include "SHT30.h"

SHT30::SHT30()
{
	IIC		= NULL;
	//Address	= 0;
	//Address	= 0xb8;
	Address	= 0x44;
	//Address	= 0x23;
}

SHT30::~SHT30()
{
	delete IIC;
	IIC = NULL;
}

/*void SHT30::Init()
{
	Write(CMD_PWN_ON);	// 打开电源
	Write(CMD_RESET);	// 软重启
	Write(0x42);
	Write(0x65);		// 设置透光率为100%
	//Write(CMD_HRES);	// 设置为高精度模式
	Write(CMD_ORES);	// 设置为高精度模式
}*/

ushort SHT30::Read()
{
	if(!IIC) return 0;

	ushort n = 0;
	IIC->Address = Address | 0x01;
	IIC->Read(0, (byte*)&n, 2);

	Sys.Sleep(5);

	return n;
}

void SHT30::Write(byte cmd)
{
	IIC->Address = Address & 0xFE;
	IIC->Write(0, &cmd, 1);

	Sys.Sleep(5);
}
