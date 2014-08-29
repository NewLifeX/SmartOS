#include "IIC_Port.h"


static I2C_TypeDef* const g_I2C_Ports[] = I2CS;


I2C_Port::I2C_Port()
{
	
}

I2C_Port::I2C_Port(int iic)
{
	_i2c = g_I2C_Ports[iic+1];
}

I2C_Port::I2C_Port(I2C_TypeDef* iic)
{
	_i2c = iic;
}
