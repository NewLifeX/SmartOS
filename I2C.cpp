#include "I2C.h"

//static I2C_TypeDef* const g_I2Cs[] = I2CS;
//static const Pin g_I2C_Pins_Map[][2] =  I2C_PINS_FULLREMAP;

I2C::I2C()
{
	Speed	= 10000;
	Retry	= 200;
	Error	= 0;
	Address	= 0x00;
}

HardI2C::HardI2C(I2C_TypeDef* iic, uint speedHz ) : I2C()
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

	Init(_index, speedHz);
}

HardI2C::HardI2C(byte index, uint speedHz) : I2C()
{
	Init(index, speedHz);
}

void HardI2C::Init(byte index, uint speedHz)
{
	_index = index;

	I2C_TypeDef* g_I2Cs[] = I2CS;
	assert_param(_index < ArrayLength(g_I2Cs));
	_IIC = g_I2Cs[_index];

	SCL.OpenDrain = true;
	SDA.OpenDrain = true;
	Pin pins[][2] =  I2C_PINS;
	SCL.Set(pins[_index][0]);
	SDA.Set(pins[_index][1]);

	Speed	= speedHz;

    debug_printf("HardI2C%d %dHz \r\n", _index + 1, speedHz);
}

HardI2C::~HardI2C()
{
	Close();
}

void HardI2C::SetPin(Pin scl , Pin sda )
{
	SCL.Set(scl);
	SDA.Set(sda);
}

void HardI2C::GetPin(Pin* scl , Pin* sda )
{
	if(scl) *scl = SCL._Pin;
	if(sda) *sda = SDA._Pin;
}

void HardI2C::Open()
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 << _index, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);

	//RCC_APB1PeriphResetCmd(sEE_I2C_CLK, ENABLE);
	//RCC_APB1PeriphResetCmd(sEE_I2C_CLK, DISABLE);

#ifdef STM32F0
	// 都在GPIO_AF_0分组内
	SCL.AFConfig(GPIO_AF_0);
	SDA.AFConfig(GPIO_AF_0);
#elif defined(STM32F4)
	byte afs[] = { GPIO_AF_I2C1, GPIO_AF_I2C2, GPIO_AF_I2C3 };
	SCL.AFConfig(afs[_index]);
	SDA.AFConfig(afs[_index]);
#endif
	SCL.Config(true);
	SDA.Config(true);

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
	i2c.I2C_Mode		= I2C_Mode_I2C;		//设置I2C接口模式
#ifndef STM32F0
    i2c.I2C_DutyCycle	= I2C_DutyCycle_2;	//设置I2C接口的高低电平周期
#endif
	i2c.I2C_OwnAddress1	= Address;			//设置I2C接口的主机地址
	i2c.I2C_Ack			= I2C_Ack_Enable;	//设置是否开启ACK响应
	i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
	i2c.I2C_ClockSpeed	= Speed;			//速度

	I2C_Cmd(_IIC, ENABLE);
	I2C_Init(_IIC, &i2c);
}

void HardI2C::Close()
{
	// sEE_I2C Peripheral Disable
	I2C_Cmd(_IIC, DISABLE);

	// sEE_I2C DeInit
	I2C_DeInit(_IIC);

	// sEE_I2C Periph clock disable
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 << _index, DISABLE);

	SCL.Config(false);
	SDA.Config(false);
}

bool HardI2C::WaitForEvent(uint event)
{
	int retry = Retry;
	while(!I2C_CheckEvent(_IIC, event));
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }
	return retry > 0;
}

bool HardI2C::SetID(byte id, bool tx)
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

void HardI2C::Write(byte id, byte addr, byte dat)
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

byte HardI2C::Read(byte id, byte addr)
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

SoftI2C::SoftI2C(uint speedHz) : I2C()
{
	Speed = speedHz;
	_delay = Sys.Clock/speedHz;
	Retry = 200;
	Error = 0;
	Address = 0x00;
}

SoftI2C::~SoftI2C()
{
	Close();
}

void SoftI2C::SetPin(Pin scl , Pin sda )
{
	SCL.Set(scl);
	SDA.Set(sda);
}

void SoftI2C::GetPin(Pin* scl , Pin* sda )
{
	if(scl) *scl = SCL._Pin;
	if(sda) *sda = SDA._Pin;
}

void SoftI2C::Open()
{
	SCL.Config(true);
	SDA.Config(true);
}

void SoftI2C::Close()
{
	SCL.Config(false);
	SDA.Config(false);
}

/*
sda		   ----____
scl		___-------____
*/
void SoftI2C::Start()
{
	SDA = true;   //发送起始条件的数据信号
	__nop();
	__nop();
	__nop();
	SCL = true;	//起始条件建立时间大于4.7us,延时
	Sys.Delay(_delay);
	SDA = false;     //发送起始信号
	Sys.Delay(_delay);
	SCL = false;    //钳住I2C总线，准备发送或接收数据
	__nop();
	__nop();
	__nop();
	__nop();
}

/*
sda		   ____----
scl		___-------____
*/
void SoftI2C::Stop()
{
	SDA = false;    //发送结束条件的数据信号
				//发送结束条件的时钟信号
	Sys.Delay(_delay);
	SCL = true;    //结束条件建立时间大于4μ
	Sys.Delay(_delay);
	SDA = true;    //发送I2C总线结束信号
	__nop();
	__nop();
	__nop();
	__nop();
}


void SoftI2C::Ack()
{
	SDA = false;
	__nop();
	__nop();
	__nop();
	__nop();
	__nop();
	SCL = true;					//时钟低电平周期大于4μ
	Sys.Delay(_delay);
	SCL = false;               //清时钟线，钳住I2C总线以便继续接收
	__nop();
	__nop();
	__nop();
	__nop();
}


byte SoftI2C::ReadData()
{
	byte rs = 0;
	SDA = true;             // 开
	int d2 = _delay >> 1;
	for(int i=0; i<8; i++)
	{
		Sys.Delay(d2);
		SCL = false;       // 置时钟线为低，准备接收数据位
		Sys.Delay(_delay);
		SCL = true;       // 置时钟线为高使数据线上数据有效
		Sys.Delay(d2);
		rs = rs << 1;
		if(SDA) rs++; //读数据位,接收的数据位放入retc中
	}
	Sys.Delay(d2);
	SCL = false;
	__nop();
	__nop();
	__nop();

	return(rs);
}

bool SoftI2C::WriteData(byte dat)
{
	for(int i=0;i<8;i++)  //要传送的数据长度为8位
    {
		if((dat << i) & 0x80)
			SDA = true;   //判断发送位
		else
			SDA = false;

		__nop();
		SCL = true;               //置时钟线为高，通知被控器开始接收数据位
		Sys.Delay(_delay);
		SCL = false;
    }
	Sys.Delay(_delay/2);
    SDA = true;               //8位发送完后释放数据线，准备接收应答位
	__nop();
	__nop();
	__nop();
	__nop();
	__nop();
    SCL = true;
	Sys.Delay(_delay/2);

	//判断是否接收到应答信号
	bool ack = SDA;

    SCL = false;
	__nop();
	__nop();
	__nop();
	__nop();
	
	return ack;
}

bool SoftI2C::SetID(byte id, bool write)
{
	return false;
}

void SoftI2C::Write(byte id, byte addr, byte dat)
{
}

byte SoftI2C::Read(byte id, byte addr)
{
	return 0;
}
