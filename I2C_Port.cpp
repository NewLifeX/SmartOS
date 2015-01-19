#include "I2C_Port.h"

//static I2C_TypeDef* const g_I2Cs[] = I2CS;
//static const Pin g_I2C_Pins_Map[][2] =  I2C_PINS_FULLREMAP;

I2C_Port::I2C_Port(I2C_TypeDef* iic, uint speedHz )
{
	assert_param(iic);

	I2C_TypeDef* g_I2Cs[] = I2CS;
	_index = 0xFF;
	for(int i=0; i<ArrayLength(g_I2Cs); i++)
	{
		if(g_I2Cs[i] == iic)
		{
			_index = i;
			break;
		}
	}
	assert_param(_index < ArrayLength(g_I2Cs));

	_IIC = g_I2Cs[_index];

	Pin pins[][2] =  I2C_PINS;
	Pins[0] = pins[_index][0];
	Pins[1] = pins[_index][1];

    debug_printf("I2C%d %dHz \r\n", _index + 1, speedHz);

	Speed = speedHz;
	Retry = 200;
	Error = 0;
	Address = 0x00;
}

I2C_Port::~I2C_Port()
{
	Close();
}

void I2C_Port::SetPin(Pin acl , Pin sda )
{
	Pins[0] = acl;
	Pins[1] = sda;
}

void I2C_Port::GetPin(Pin* acl , Pin* sda )
{
	*acl = Pins[0];
	*sda = Pins[1];
}

void I2C_Port::Open()
{
	// gpio  复用开漏输出
	SCL = new AlternatePort(Pins[0], true);
	SDA = new AlternatePort(Pins[1], true);

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 << _index, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	//RCC_APB1PeriphResetCmd(sEE_I2C_CLK, ENABLE);
	//RCC_APB1PeriphResetCmd(sEE_I2C_CLK, DISABLE);

#if defined(STM32F0)
	// SPI都在GPIO_AF_0分组内
    GPIO_PinAFConfig(_GROUP(Pins[0]), _PIN(Pins[0]), GPIO_AF_0);
    GPIO_PinAFConfig(_GROUP(Pins[1]), _PIN(Pins[1]), GPIO_AF_0);
#elif defined(STM32F4)
	byte afs[] = { GPIO_AF_I2C1, GPIO_AF_I2C2, GPIO_AF_I2C3 };
    GPIO_PinAFConfig(_GROUP(Pins[0]), _PIN(Pins[0]), afs[_index]);
    GPIO_PinAFConfig(_GROUP(Pins[1]), _PIN(Pins[1]), afs[_index]);
#endif

	I2C_InitTypeDef i2c;
	I2C_StructInit(&i2c);
	I2C_DeInit(_IIC);	// 复位

//	I2C_Timing
//	I2C_AnalogFilter
//	I2C_DigitalFilter
//	I2C_Mode
//	I2C_OwnAddress1
//	I2C_Ack
//	I2C_AcknowledgedAddress

	/*i2c.I2C_AnalogFilter = I2C_AnalogFilter_Disable;	// 关闭滤波器
//	i2c.I2C_DigitalFilter		// 数字滤波器  不知道怎么设置 跟 cr1 的8-11位有关
	i2c.I2C_Mode =I2C_Mode_I2C;	// IIC 模式
//	i2c.I2C_OwnAddress1
	i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	if(addressLen == ADDR_LEN_10)
		i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_10bit;*/
	i2c.I2C_Mode = I2C_Mode_I2C;
	i2c.I2C_DutyCycle = I2C_DutyCycle_2;
	i2c.I2C_OwnAddress1 = Address;
	i2c.I2C_Ack = I2C_Ack_Enable;
	i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	i2c.I2C_ClockSpeed = Speed;

	I2C_Cmd(_IIC, ENABLE);
	I2C_Init(_IIC, &i2c);
}

void I2C_Port::Close()
{
	// sEE_I2C Peripheral Disable
	I2C_Cmd(_IIC, DISABLE);

	// sEE_I2C DeInit
	I2C_DeInit(_IIC);

	// sEE_I2C Periph clock disable
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 << _index, DISABLE);

	if(SCL) delete SCL;
	SCL = NULL;

	if(SDA) delete SDA;
	SDA = NULL;
}

bool I2C_Port::WaitForEvent(uint event)
{
	int retry = Retry;
	while(!I2C_CheckEvent(_IIC, event));
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }
	return retry > 0;
}

bool I2C_Port::SetID(byte id, bool tx)
{
	// 产生起始条件
	I2C_GenerateSTART(_IIC, ENABLE);
	// 等待ACK
	if(!WaitForEvent(I2C_EVENT_MASTER_MODE_SELECT)) return false;
	if(tx)
	{
		// 向设备发送设备地址
		I2C_Send7bitAddress(_IIC, id, I2C_Direction_Transmitter);
		// 等待ACK
		if(!WaitForEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) return false;
	}
	else
	{
		// 发送读地址
		I2C_Send7bitAddress(_IIC, id, I2C_Direction_Receiver);
		// EV6
		if(!WaitForEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED)) return 0;
	}

	return true;
}

void I2C_Port::Write(byte id, byte addr, byte dat)
{
	if(!SetID(id)) return;

	// 寄存器地址
	I2C_SendData(_IIC, addr);
	// 等待ACK
	if(!WaitForEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return;
	// 发送数据
	I2C_SendData(_IIC, dat);
	// 发送完成
	if(!WaitForEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return;
	// 产生结束信号
	I2C_GenerateSTOP(_IIC, ENABLE);
}

byte I2C_Port::Read(byte id, byte addr)
{
	// 等待I2C
	while(I2C_GetFlagStatus(_IIC, I2C_FLAG_BUSY));

	if(!SetID(id)) return 0;

	// 重新设置可以清楚EV6
	I2C_Cmd(_IIC, ENABLE);
	// 发送读得地址
	I2C_SendData(_IIC, addr);
	// EV8
	if(!WaitForEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED)) return 0;

	if(!SetID(id, false)) return 0;

	// 关闭应答
	I2C_AcknowledgeConfig(_IIC, DISABLE);
	// 停止条件产生
	I2C_GenerateSTOP(_IIC, ENABLE);
	if(!WaitForEvent(I2C_EVENT_MASTER_BYTE_RECEIVED)) return 0;

	byte dat = I2C_ReceiveData(_IIC);

	I2C_AcknowledgeConfig(_IIC, ENABLE);

	return dat;
}
