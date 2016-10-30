#include "Sys.h"
#include "Device\I2C.h"

#include "Platform\stm32.h"

void HardI2C::OnInit()
{
	I2C_TypeDef* g_I2Cs[] = I2CS;
	assert(_index < ArrayLength(g_I2Cs), "I2C::Init");
	_IIC = g_I2Cs[_index];

	SCL.OpenDrain = true;
	SDA.OpenDrain = true;
	Pin pins[][2] =  I2C_PINS;
	SCL.Set(pins[_index][0]);
	SDA.Set(pins[_index][1]);
}

void HardI2C::OnOpen()
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 << _index, ENABLE);
#ifdef STM32F0
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
#endif

	//RCC_APB1PeriphResetCmd(sEE_I2C_CLK, ENABLE);
	//RCC_APB1PeriphResetCmd(sEE_I2C_CLK, DISABLE);

#ifdef STM32F0
	// 都在GPIO_AF_0分组内
	SCL.AFConfig(Port::AF_0);
	SDA.AFConfig(Port::AF_0);
#elif defined(STM32F4)
	byte afs[] = { GPIO_AF_I2C1, GPIO_AF_I2C2, GPIO_AF_I2C3 };
	SCL.AFConfig((Port::GPIO_AF)afs[_index]);
	SDA.AFConfig((Port::GPIO_AF)afs[_index]);
#endif
	SCL.Open();
	SDA.Open();

	auto iic	= (I2C_TypeDef*)_IIC;
	I2C_InitTypeDef i2c;
	I2C_StructInit(&i2c);

	I2C_DeInit(iic);	// 复位

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
	i2c.I2C_Mode		= I2C_Mode_I2C;		//设置I2C接口模式
#ifndef STM32F0
    i2c.I2C_DutyCycle	= I2C_DutyCycle_2;	//设置I2C接口的高低电平周期
#endif
	i2c.I2C_OwnAddress1	= Address;			//设置I2C接口的主机地址。从设备需要设定的主机地址，而对于主机而言这里无所谓
	i2c.I2C_Ack			= I2C_Ack_Enable;	//设置是否开启ACK响应
	i2c.I2C_ClockSpeed	= Speed;			//速度

	if(Address > 0xFF)
		i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_10bit;
	else
		i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;

	I2C_Cmd(iic, ENABLE);
	I2C_Init(iic, &i2c);
}

void HardI2C::OnClose()
{
	auto iic	= (I2C_TypeDef*)_IIC;
	// sEE_I2C Peripheral Disable
	I2C_Cmd(iic, DISABLE);

	// sEE_I2C DeInit
	I2C_DeInit(iic);

	// sEE_I2C Periph clock disable
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 << _index, DISABLE);

	SCL.Close();
	SDA.Close();
}

void HardI2C::Start()
{
	auto iic	= (I2C_TypeDef*)_IIC;
	// 允许1字节1应答模式
	I2C_AcknowledgeConfig(iic, ENABLE);
    I2C_GenerateSTART(iic, ENABLE);

	_Event = I2C_EVENT_MASTER_MODE_SELECT;
	WaitAck();
}

void HardI2C::Stop()
{
	auto iic	= (I2C_TypeDef*)_IIC;
	// 最后一位后要关闭应答
	I2C_AcknowledgeConfig(iic, DISABLE);
	// 产生结束信号
	I2C_GenerateSTOP(iic, ENABLE);
}

void HardI2C::Ack(bool ack)
{
	I2C_AcknowledgeConfig((I2C_TypeDef*)_IIC, ack ? ENABLE : DISABLE);
}

bool HardI2C::WaitAck(int retry)
{
	if(!retry) retry = Retry;
	while(!I2C_CheckEvent((I2C_TypeDef*)_IIC, _Event))
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }
	return retry > 0;
}

bool HardI2C::SendAddress(int addr, bool tx)
{
	auto iic	= (I2C_TypeDef*)_IIC;
	if(tx)
	{
		// 向设备发送设备地址
		I2C_Send7bitAddress(iic, Address, I2C_Direction_Transmitter);

		_Event = I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED;
		if(!WaitAck()) return false;

		_Event = I2C_EVENT_MASTER_BYTE_TRANSMITTED;
		return I2C::SendSubAddr(addr);
	}
	else
	{
		// 如果有子地址，需要先发送子地址。而子地址作为数据发送，需要首先设置为发送模式
		if(SubWidth > 0)
		{
			if(!SendAddress(addr, true)) return false;

			// 重启会话
			Start();
		}

		// 发送读地址
		I2C_Send7bitAddress(iic, Address, I2C_Direction_Receiver);

		_Event = I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED;
		if(!WaitAck()) return false;

		return true;
	}
}

void HardI2C::WriteByte(byte dat)
{
	_Event = I2C_EVENT_MASTER_BYTE_TRANSMITTED;

	I2C_SendData((I2C_TypeDef*)_IIC, dat);
}

byte HardI2C::ReadByte()
{
	_Event = I2C_EVENT_MASTER_BYTE_RECEIVED;

	return I2C_ReceiveData((I2C_TypeDef*)_IIC);
}

/*
SDA 和SCL 都是双向线路都通过一个电流源或上拉电阻连接到正的电源电压。
当总线空闲时这两条线路都是高电平。

SDA 线上的数据必须在时钟的高电平周期保持稳定。
数据线的高或低电平状态只有在SCL线的时钟信号是低电平时才能改变。
起始和停止例外，因此从机很容易区分起始和停止信号。
*/
