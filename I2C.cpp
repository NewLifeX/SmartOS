#include "I2C.h"

//static I2C_TypeDef* const g_I2Cs[] = I2CS;
//static const Pin g_I2C_Pins_Map[][2] =  I2C_PINS_FULLREMAP;

I2C::I2C()
{
	Speed	= 10000;
	Retry	= 200;
	Error	= 0;

	Address	= 0x00;
	SubWidth= 0;

	Opened	= false;
}

I2C::~I2C()
{
	Close();
}

void I2C::Open()
{
	if(Opened) return;

	OnOpen();

	Opened = true;
}

void I2C::Close()
{
	if(!Opened) return;

	OnClose();

	Opened = false;
}

bool I2C::SendAddress(int addr, bool tx)
{
	// 发送设备地址
    WriteByte(Address);
	if(!WaitAck()) return false;

	return SendSubAddr(addr);
}

bool I2C::SendSubAddr(int addr)
{
	// 发送子地址
	if(SubWidth > 0)
	{
		// 逐字节发送
		for(int k=SubWidth-1; k>=0; k--)
		{
			WriteByte(addr >> (k << 3));
			if(!WaitAck()) return false;
		}
	}

	return true;
}

// 新会话向指定地址写入多个字节
bool I2C::Write(int addr, byte* buf, uint len)
{
	Open();

	I2CScope ics(this);

	// 发送设备地址
    if(!SendAddress(addr)) return false;

	for(int i=0; i<len; i++)
	{
		WriteByte(*buf++);
		if(!WaitAck()) return false;
	}

	return true;
}

// 新会话从指定地址读取多个字节
uint I2C::Read(int addr, byte* buf, uint len)
{
	Open();

	I2CScope ics(this);

	// 发送设备地址
    if(!SendAddress(addr)) return 0;

	uint rs = 0;
	for(int i=0; i<len; i++)
	{
		*buf++ = ReadByte();
		rs++;
		Ack(i < len - 1);	// 最后一次不需要发送Ack
	}
	return rs;
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

    debug_printf("HardI2C_%d::Init %dHz \r\n", _index + 1, speedHz);
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
	SCL.AFConfig(GPIO_AF_0);
	SDA.AFConfig(GPIO_AF_0);
#elif defined(STM32F4)
	byte afs[] = { GPIO_AF_I2C1, GPIO_AF_I2C2, GPIO_AF_I2C3 };
	SCL.AFConfig(afs[_index]);
	SDA.AFConfig(afs[_index]);
#endif
	SCL.Open();
	SDA.Open();

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
	i2c.I2C_OwnAddress1	= Address;			//设置I2C接口的主机地址。从设备需要设定的主机地址，而对于主机而言这里无所谓
	i2c.I2C_Ack			= I2C_Ack_Enable;	//设置是否开启ACK响应
	i2c.I2C_ClockSpeed	= Speed;			//速度

	if(Address > 0xFF)
		i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_10bit;
	else
		i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;

	I2C_Cmd(_IIC, ENABLE);
	I2C_Init(_IIC, &i2c);
}

void HardI2C::OnClose()
{
	// sEE_I2C Peripheral Disable
	I2C_Cmd(_IIC, DISABLE);

	// sEE_I2C DeInit
	I2C_DeInit(_IIC);

	// sEE_I2C Periph clock disable
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1 << _index, DISABLE);

	SCL.Close();
	SDA.Close();
}

void HardI2C::Start()
{
	// 允许1字节1应答模式
	I2C_AcknowledgeConfig(_IIC, ENABLE);
    I2C_GenerateSTART(_IIC, ENABLE);

	_Event = I2C_EVENT_MASTER_MODE_SELECT;
	WaitAck();
}

void HardI2C::Stop()
{
	// 最后一位后要关闭应答
	I2C_AcknowledgeConfig(_IIC, DISABLE);
	// 产生结束信号
	I2C_GenerateSTOP(_IIC, ENABLE);
}

void HardI2C::Ack(bool ack)
{
	I2C_AcknowledgeConfig(_IIC, ack ? ENABLE : DISABLE);
}

bool HardI2C::WaitAck(int retry)
{
	if(!retry) retry = Retry;
	while(!I2C_CheckEvent(_IIC, _Event))
    {
        if(--retry <= 0) return ++Error; // 超时处理
    }
	return retry > 0;
}

bool HardI2C::SendAddress(int addr, bool tx)
{
	if(tx)
	{
		// 向设备发送设备地址
		I2C_Send7bitAddress(_IIC, Address, I2C_Direction_Transmitter);

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
		I2C_Send7bitAddress(_IIC, Address, I2C_Direction_Receiver);

		_Event = I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED;
		if(!WaitAck()) return false;

		return true;
	}
}

void HardI2C::WriteByte(byte dat)
{
	_Event = I2C_EVENT_MASTER_BYTE_TRANSMITTED;

	I2C_SendData(_IIC, dat);
}

byte HardI2C::ReadByte()
{
	_Event = I2C_EVENT_MASTER_BYTE_RECEIVED;

	return I2C_ReceiveData(_IIC);
}

/*// 新会话向指定地址写入多个字节
bool HardI2C::Write(int addr, byte* buf, uint len)
{
	Open();

	I2CScope ics(this);

	_Event = I2C_EVENT_MASTER_MODE_SELECT;
	if(!WaitAck()) return false;

	// 发送设备地址
	SendAddress(addr, true);

	_Event = I2C_EVENT_MASTER_BYTE_TRANSMITTED;

	//发送数据
	while(len--){
	  	I2C_SendData(_IIC, *buf++);

		if(!WaitAck()) return false;
	}

	return true;
}

// 新会话从指定地址读取多个字节
uint HardI2C::Read(int addr, byte* buf, uint len)
{
	_Event = I2C_FLAG_BUSY;
	if(!WaitAck()) return false;

	I2CScope ics(this);

	_Event = I2C_EVENT_MASTER_MODE_SELECT;
	if(!WaitAck()) return false;

	// 发送设备地址
	SendAddress(addr, false);

	//读取数据
	_Event = I2C_EVENT_MASTER_BYTE_RECEIVED;
    while (len)
    {
		if(len==1)
		{
     		I2C_AcknowledgeConfig(_IIC, DISABLE);
    		I2C_GenerateSTOP(_IIC, ENABLE);
		}
		if(!WaitAck()) return false;

	    *buf++ = I2C_ReceiveData(_IIC);

	    len--;
    }

	I2C_AcknowledgeConfig(_IIC, ENABLE);

	return 0;
}*/

SoftI2C::SoftI2C(uint speedHz) : I2C()
{
	Speed = speedHz;
	_delay = Sys.Clock / speedHz;
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

void SoftI2C::OnOpen()
{
	assert_param2(!SCL.Empty() && !SDA.Empty(), "未设置I2C引脚");

	debug_printf("I2C::Open Address=0x%02X \r\n", Address);

	// 开漏输出
	SCL.OpenDrain = true;
	SDA.OpenDrain = true;

	SCL.Open();
	SDA.Open();
}

void SoftI2C::OnClose()
{
	SCL.Close();
	SDA.Close();
}

/*
sda		   ----____
scl		___--------____
*/
void SoftI2C::Start()
{
	SDA = true;		//发送起始条件的数据信号
	__nop();
	__nop();
	__nop();
	SCL = true;		//起始条件建立时间大于4.7us,延时
	Sys.Delay(4);
	SDA = false;	//发送起始信号
	Sys.Delay(4);
	SCL = false;	//钳住I2C总线，准备发送或接收数据
	__nop();
	__nop();
	__nop();
	__nop();
}

/*
sda		____----
scl		____----
*/
void SoftI2C::Stop()
{
	SCL = false;	//发送结束条件的时钟信号
	SDA = false;    //发送结束条件的数据信号
	Sys.Delay(4);
	SCL = true;    //结束条件建立时间大于4μ
	SDA = true;    //发送I2C总线结束信号
	Sys.Delay(4);
}

// 等待Ack
bool SoftI2C::WaitAck(int retry)
{
	SDA = true;
	Sys.Delay(1);
	SCL = true;
	Sys.Delay(1);

	// 等待SDA低电平
	if(!retry) retry = Retry;
	while(SDA)
	{
		if(retry-- <= 0)
		{
			Stop();
			return false;
		}
	}

	SCL = false;

	return true;
}

// 发送Ack
void SoftI2C::Ack(bool ack)
{
	SCL = false;	//时钟低电平周期大于4μ
	SDA = !ack;
	Sys.Delay(2);
	SCL = true;		//清时钟线，钳住I2C总线以便继续接收
	Sys.Delay(2);
	SCL = false;
}

void SoftI2C::WriteByte(byte dat)
{
	SCL = false;	//拉低时钟开始数据传输
	for(int i=0; i<8; i++)  //要传送的数据长度为8位
    {
		SDA = (dat & 0x80) >> 7;   //判断发送位
		dat <<= 1;

		Sys.Delay(2);
		SCL = true;               //置时钟线为高，通知被控器开始接收数据位
		Sys.Delay(2);
		SCL = false;
		Sys.Delay(2);
    }
}

byte SoftI2C::ReadByte()
{
	byte rs = 0;
	SDA = true;             // 开
	for(int i=0; i<8; i++)
	{
		SCL = false;	// 置时钟线为低，准备接收数据位
		Sys.Delay(2);
		SCL = true;		// 置时钟线为高使数据线上数据有效
		rs = rs << 1;
		if(SDA) rs++;	//读数据位,接收的数据位放入retc中
		Sys.Delay(1);
	}

	return rs;
}
