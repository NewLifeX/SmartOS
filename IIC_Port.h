#ifndef _I2C_PORT_H_
#define _I2C_PORT_H_

#include "Sys.h"
#include "Port.h"


//SCL		开漏复用输出
//SDA		开漏复用输出


class I2C_Port
{
public:
	I2C_Port();
	I2C_Port(int iic);
	I2C_Port(I2C_TypeDef* iic);

private:
	int _iic;
//	int _baudRate;
//	int _parity;
//	int _dataBits;
//	int _stopBits;
	
	
    I2C_TypeDef* _i2c;
	AlternatePort* SCL;
	AlternatePort* SDA;

	~I2C_Port();
};

#endif
