#include "BH1750.h"

#define	CMD_PWN_OFF	0x00
#define	CMD_PWN_ON	0x01
#define	CMD_RESET	0x07

// 连续分辨率
#define CMD_HRES	0x10	// 高解析度 1	测量时间120ms
#define CMD_HRES2	0x11	// 高解析度 0.5	测量时间120ms
//#define CMD_MRES	0x13
#define CMD_LRES	0x13	// 高解析度 4	测量时间16ms

BH1750::BH1750()
{
	IIC		= NULL;
	//Address	= 0;
	//Address	= 0xb8;
	Address	= 0x46;
	//Address	= 0x23;
}

BH1750::~BH1750()
{
	delete IIC;
	IIC = NULL;
}

void BH1750::Init()
{
	Write(CMD_PWN_ON);	// 打开电源
	Write(CMD_RESET);	// 软重启
	Write(0x42);
	Write(0x65);		// 设置透光率为100%
	Write(CMD_HRES);	// 设置为高精度模式
}

ushort BH1750::Read()
{
	/*ushort n = 0;

	IIC->Start();

	IIC->Write(Address);
	if (!IIC->WaitAck()) return 0;

	n |= IIC->Read() << 8;
	IIC->Ack();
	n |= IIC->Read();

	IIC->Stop();
	Sys.Sleep(5);*/
	
	ushort n = 0;
	IIC->Read(Address, (byte*)&n, 2);
	
	Sys.Sleep(5);
	
	return n;
}

void BH1750::Write(byte cmd)
{
	/*IIC->Start();

    IIC->Write(Address);   //发送设备地址+写信号
	while(!IIC->WaitAck());

	IIC->Write(cmd);    //内部寄存器地址
	while(!IIC->WaitAck());
    //BH1750_SendByte(REG_data);       //内部寄存器数据，

	IIC->Stop();
	Sys.Sleep(5);*/
	
	IIC->Write(Address, &cmd, 1);
	
	Sys.Sleep(5);
}
