#include "I2C_Port.h"


static I2C_TypeDef* const g_I2Cs[] = I2CS;
static const Pin g_I2C_Pins_Map[][2] =  I2C_PINS_FULLREMAP;

/*
#define IIC1	0
#define IIC2	1
*/



int GetPre(int iic, uint* speedHz)					// 计算频率
{
	return true;
}



I2C_Port::I2C_Port(int iic, uint speedHz )
{
	assert_param(iic >= 0 && iic < ArrayLength(g_I2Cs));
	_i2c = iic;
	_IIC = g_I2Cs[iic];
	
	Pins[0] = g_I2C_Pins_Map[iic][0];
	Pins[1] = g_I2C_Pins_Map[iic][1];
	
    debug_printf("I2C%d %dHz \r\n", iic + 1, speedHz);
	
	Speed = speedHz;
	Retry = 200;
}



void I2C_Port::SetPin(Pin acl , Pin sda )
{
	Pins[0] = acl;
	Pins[1] = sda;
}

void I2C_Port::GetPin(Pin* acl , Pin* sda )
{
	* acl = Pins[0];
	* sda = Pins[1];
}


void I2C_Port::Open()
{
	
	// gpio  复用开漏输出
	SCL = new AlternatePort(Pins[0],true);
	SDA = new AlternatePort(Pins[1],true);
	
	I2C_InitTypeDef I2C_InitStruct;
	I2C_DeInit(_IIC);	// 复位
	  I2C_InitStruct.I2C_Timing =
	I2C_Init(_IIC, & I2C_InitStruct);
}




void I2C_Port::Close()
{
}







