#ifndef _I2C_H_
#define _I2C_H_

#include "Sys.h"
#include "Port.h"


//SCL		开漏复用输出
//SDA		开漏复用输出


class I2C
{
private:
	void Init(byte index, uint speedHz);
	
public:
    int Speed;		// 速度
    int Retry;		// 等待重试次数，默认200
    int Error;		// 错误次数
	ushort Address;	// 7位地址或10位地址

	// 使用端口和最大速度初始化，因为需要分频，实际速度小于等于该速度
    I2C(I2C_TypeDef* iic = I2C1, uint speedHz = 10000);
	I2C(byte index, uint speedHz = 10000);
    virtual ~I2C();

	void SetPin(Pin scl = P0, Pin sda = P0);
	void GetPin(Pin* scl = NULL, Pin* sda = NULL);

	void Open();
	void Close();

	void Write(byte id, byte addr, byte dat);
	byte Read(byte id, byte addr);

private:
    byte _index;
	I2C_TypeDef* _IIC;

	Pin Pins[2];
	AlternatePort* SCL;
	AlternatePort* SDA;

	bool WaitForEvent(uint event);
	bool SetID(byte id, bool tx = true);
};

#endif
