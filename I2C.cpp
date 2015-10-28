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

#pragma arm section code = "SectionForSys"

/*
主机与从机进行通信时，有时需要切换数据的收发方向。例如，访问某一具有I2C 总线
接口的E2PROM 存储器时，主机先向存储器输入存储单元的地址信息（发送数据），然后再
读取其中的存储内容（接收数据）。
在切换数据的传输方向时，可以不必先产生停止条件再开始下次传输，而是直接再一次
产生开始条件。I2C 总线在已经处于忙的状态下，再一次直接产生起始条件的情况被称为重
复起始条件。重复起始条件常常简记为Sr。
正常的起始条件和重复起始条件在物理波形上并没有什么不同，区别仅仅是在逻辑方
面。在进行多字节数据传输过程中，只要数据的收发方向发生了切换，就要用到重复起始条
件。
*/
bool I2C::SendAddress(int addr, bool tx)
{
	// 1，写入模式，不管有没有子地址，先发送写地址，再发送子地址
	// 2，读取模式，如果没有子地址，先发送读地址，再直接读取
	// 3，读取模式，如果有子地址，先发送写地址，再发送子地址，然后重新开始并发送读地址

	// 发送写入地址
	ushort d = (tx || SubWidth > 0) ? Address : (Address | 0x01);
    WriteByte(d);
	if(!WaitAck())
	{
		debug_printf("I2C::SendAddress 可能设备未连接，或地址 0x%02X 不对 \r\n", d);
		return false;
	}

	if(!SubWidth) return true;

	// 发送子地址
	if(!SendSubAddr(addr))
	{
		debug_printf("I2C::SendAddress 发送子地址 0x%02X 失败 \r\n", addr);
		return false;
	}

	if(tx) return true;

	d = Address | 0x01;
	// 多次尝试启动并发送读取地址
	uint retry = 10;
	bool rs = false;
	while(retry-- && !rs)
	{
		Start();
		WriteByte(d);
		rs = WaitAck();
	}
	if(!rs)
	{
		debug_printf("I2C::SendAddress 发送读取地址 0x%02X 失败 \r\n", d);
		return false;
	}

	return rs;
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
bool I2C::Write(int addr, const ByteArray& bs)
{
	/*debug_printf("I2C::Write addr=0x%02X ", addr);
	bs.Show(true);*/

	Open();

	I2CScope ics(this);

	// 发送设备地址
    if(!SendAddress(addr, true)) return false;

	uint len = bs.Length();
	for(int i=0; i<len; i++)
	{
		WriteByte(bs[i]);
		// 最后一次不要等待Ack
		if(i < len - 1 && !WaitAck()) return false;
	}

	return true;
}

// 新会话从指定地址读取多个字节
uint I2C::Read(int addr, ByteArray& bs)
{
	Open();

	I2CScope ics(this);

	// 发送设备地址
    if(!SendAddress(addr, false)) return 0;

	uint rs = 0;
	uint len = bs.Length();
	for(int i=0; i<len; i++)
	{
		bs[i] = ReadByte();
		rs++;
		Ack(i < len - 1);	// 最后一次不需要发送Ack
	}
	return rs;
}

bool I2C::Write(int addr, byte data) { return Write(addr, ByteArray(&data, 1)); }
byte I2C::Read(int addr)
{
	ByteArray bs(1);
	if(!Read(addr, bs)) return 0;

	return bs[0];
}
ushort I2C::Read2(int addr)
{
	ByteArray bs(2);
	if(!Read(addr, bs)) return 0;

	return (bs[0] << 8) | bs[1];
}
uint I2C::Read4(int addr)
{
	ByteArray bs(4);
	if(!Read(addr, bs)) return 0;

	return (bs[0] << 24) | (bs[1] << 16) | (bs[2] << 8) | bs[3];
}

#pragma arm section code

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
	Speed	= speedHz;
	_delay	= Sys.Clock / speedHz;
	Retry	= 100;
	Error	= 0;
	Address	= 0x00;
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

	// 当总线空闲时这两条线路都是高电平
	SCL = true;
	SDA = true;
}

void SoftI2C::OnClose()
{
	SCL.Close();
	SDA.Close();
}

#pragma arm section code = "SectionForSys"

void SoftI2C::Delay(int us)
{
	Sys.Delay(us);
}

// 起始条件 当 SCL 处于高电平期间时，SDA 从高电平向低电平跳变时产生起始条件。
// 总线在起始条件产生后便处于忙的状态。起始条件常常简记为S。
/*
sda		   ----____
scl		___--------____
*/
void SoftI2C::Start()
{
	// 在SCL高电平期间，SDA产生下降沿
	SCL = true;

	// 下降沿
	SDA = true;
	Delay(1);
	SDA = false;

	Delay(1);
	SCL = false;
}

// 停止条件 当 SCL 处于高电平期间时，SDA 从低电平向高电平跳变时产生停止条件。
// 总线在停止条件产生后处于空闲状态。停止条件简记为P。
/*
sda		____----
scl		____----
*/
void SoftI2C::Stop()
{
	// 在SCL高电平期间，SDA产生上升沿
	SCL = false;
	SDA = false;
	Delay(1);

	SCL = true;
	Delay(1);
	SDA = true;
}

// 等待Ack
bool SoftI2C::WaitAck(int retry)
{
	if(!retry) retry = Retry;

	// SDA 线上的数据必须在时钟的高电平周期保持稳定
	SDA = true;
	SCL = true;
	Delay(1);

	// 等待SDA低电平
	while(SDA.ReadInput())
	{
		if(retry-- <= 0)
		{
			//debug_printf("SoftI2C::WaitAck Retry=%d 无法等到ACK \r\n", Retry);
			return false;
		}
	}

	SCL = false;
	Delay(1);

	return true;
}

// 发送Ack
/*
在 I2C 总线传输数据过程中，每传输一个字节，都要跟一个应答状态位。接收器接收数
据的情况可以通过应答位来告知发送器。应答位的时钟脉冲仍由主机产生，而应答位的数据
状态则遵循“谁接收谁产生”的原则，即总是由接收器产生应答位。主机向从机发送数据时，
应答位由从机产生；主机从从机接收数据时，应答位由主机产生。I2C 总线标准规定：应答
位为0 表示接收器应答（ACK），常常简记为A；为1 则表示非应答（NACK），常常简记为
A。发送器发送完LSB 之后，应当释放SDA 线（拉高SDA，输出晶体管截止），以等待接
收器产生应答位。
*/
void SoftI2C::Ack(bool ack)
{
	// SDA 线上的数据必须在时钟的高电平周期保持稳定
	SCL = false;

	SDA = !ack;
	Delay(1);

	SCL = true;
	Delay(1);
	SCL = false;

	SDA = true;
	Delay(1);
}

void SoftI2C::WriteByte(byte dat)
{
	// SDA 线上的数据必须在时钟的高电平周期保持稳定
	SCL = false;
	for(byte mask=0x80; mask>0; mask>>=1)
    {
		SDA = dat & mask;
		Delay(1);

		// 置时钟线为高，通知被控器开始接收数据位
		SCL = true;
		Delay(1);
		SCL = false;
		Delay(1);
    }
}

byte SoftI2C::ReadByte()
{
	// SDA 线上的数据必须在时钟的高电平周期保持稳定
	SDA = true;
	byte rs = 0;
	for(byte mask=0x80; mask>0; mask>>=1)
	{
		SCL = true;		// 置时钟线为高使数据线上数据有效
		//Delay(2);
		// 等SCL变高
		uint retry = 50;
		while(!SCL.ReadInput())
		{
			if(retry-- <= 0) break;
			Delay(1);
		}

		if(SDA.ReadInput()) rs |= mask;	//读数据位
		SCL = false;	// 置时钟线为低，准备接收数据位
		Delay(1);
	}

	return rs;
}

/*
SDA 和SCL 都是双向线路都通过一个电流源或上拉电阻连接到正的电源电压。
当总线空闲时这两条线路都是高电平。

SDA 线上的数据必须在时钟的高电平周期保持稳定。
数据线的高或低电平状态只有在SCL线的时钟信号是低电平时才能改变。
起始和停止例外，因此从机很容易区分起始和停止信号。
*/
