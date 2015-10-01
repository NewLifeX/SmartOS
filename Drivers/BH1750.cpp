#include "BH1750.h"

#define	CMD_PWN_OFF	0x00
#define	CMD_PWN_ON	0x01
#define	CMD_RESET	0x07

// 连续分辨率
#define CMD_HRES	0x10	// 高解析度 1	测量时间120ms
#define CMD_HRES2	0x11	// 高解析度 0.5	测量时间120ms
//#define CMD_MRES	0x13
#define CMD_LRES	0x13	// 高解析度 4	测量时间16ms

// 一次分辨率
#define CMD_ORES	0x20	// 高解析度 1	测量时间120ms

BH1750::BH1750()
{
	IIC		= NULL;
	
	// 7位地址，到了I2C那里，需要左移1位
	Address	= 0x5C;
	//Address	= 0x23;
}

BH1750::~BH1750()
{
	delete IIC;
	IIC = NULL;
}

void BH1750::Init()
{
	debug_printf("BH1750::Init Address=0x%02X \r\n", Address);

	IIC->Address = Address << 1;

	Write(CMD_PWN_ON);	// 打开电源
	Write(CMD_RESET);	// 软重启
	//Write(0x42);
	//Write(0x65);		// 设置透光率为100%
	//Write(CMD_HRES);	// 设置为高精度模式
	Write(CMD_ORES);	// 设置为高精度模式
}

ushort BH1750::Read()
{
	if(!IIC) return 0;

	ByteArray bs(2);
	if(IIC->Read(0, bs) == 0) return 0;

	ushort n = (bs[0] << 8) | bs[1];

	return n;
}

void BH1750::Write(byte cmd)
{
	if(!IIC) return;

	IIC->Write(0, ByteArray(&cmd, 1));
	Sys.Sleep(5);
}
