#ifndef _I2C_H_
#define _I2C_H_

#include "Sys.h"
#include "Port.h"

//SCL		开漏复用输出
//SDA		开漏复用输出

// I2C外设
class I2C
{
public:
    int Speed;		// 速度
    int Retry;		// 等待重试次数，默认200
    int Error;		// 错误次数
	ushort Address;	// 7位地址或10位地址

	I2C();
	
	virtual void SetPin(Pin scl, Pin sda) = 0;
	virtual void GetPin(Pin* scl = NULL, Pin* sda = NULL) = 0;

	virtual void Open() = 0;
	virtual void Close() = 0;

	virtual void Write(byte id, byte addr, byte dat) = 0;
	virtual byte Read(byte id, byte addr) = 0;
};

// 硬件I2C
class HardI2C : public I2C
{
private:
	void Init(byte index, uint speedHz);
	
public:
	// 使用端口和最大速度初始化，因为需要分频，实际速度小于等于该速度
    HardI2C(I2C_TypeDef* iic = I2C1, uint speedHz = 10000);
	HardI2C(byte index, uint speedHz = 10000);
    virtual ~HardI2C();

	virtual void SetPin(Pin scl, Pin sda);
	virtual void GetPin(Pin* scl = NULL, Pin* sda = NULL);

	virtual void Open();
	virtual void Close();

	virtual void Write(byte id, byte addr, byte dat);
	virtual byte Read(byte id, byte addr);

private:
    byte _index;
	I2C_TypeDef* _IIC;

	AlternatePort SCL;
	AlternatePort SDA;

	bool WaitForEvent(uint event);
	bool SetID(byte id, bool tx = true);
};

// 软件模拟I2C
class SoftI2C : public I2C
{
public:
	bool HasSecAddress;	// 设备是否有子地址  

	// 使用端口和最大速度初始化，因为需要分频，实际速度小于等于该速度
    SoftI2C(uint speedHz = 10000);
    virtual ~SoftI2C();

	virtual void SetPin(Pin scl, Pin sda);
	virtual void GetPin(Pin* scl = NULL, Pin* sda = NULL);

	virtual void Open();
	virtual void Close();
	
	virtual void Write(byte id, byte addr, byte dat);
	virtual byte Read(byte id, byte addr);
	
private:
	int _delay;			// 根据速度匹配的延时

	OutputPort SCL;
	OutputPort SDA;
	
	void Start();
	void Stop();
	
	void Ack();
	byte ReadData();
	bool WriteData(byte dat);
	bool SetID(byte id, bool tx = true);
	bool WaitForEvent(uint event);
};

#endif
