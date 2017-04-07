#include "Kernel\Sys.h"

#include "I2C.h"

I2C::I2C()
{
	Speed = 10000;
	Retry = 200;
	Error = 0;

	Address = 0x00;
	SubWidth = 0;

	Opened = false;
}

I2C::~I2C()
{
	Close();
}

bool I2C::Open()
{
	if (Opened) return true;

	if (!OnOpen()) return false;

	return Opened = true;
}

void I2C::Close()
{
	if (!Opened) return;

	OnClose();

	Opened = false;
}

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
	//debug_printf("I2C::SendAddr %02X \r\n", d);
	WriteByte((byte)d);
	if (!WaitAck(true))
	{
		debug_printf("I2C::SendAddr %02X 可能设备未连接，或地址不对\r\n", d);
		return false;
	}

	if (!SubWidth) return true;

	// 发送子地址
	if (!SendSubAddr(addr))
	{
		debug_printf("I2C::SendAddr %02X 发送子地址 0x%02X 失败\r\n", d, addr);
		return false;
	}

	if (tx) return true;

	d = Address | 0x01;
	// 多次尝试启动并发送读取地址
	uint retry = 10;
	bool rs = false;
	while (retry-- && !rs)
	{
		Start();
		WriteByte((byte)d);
		rs = WaitAck(true);
	}
	if (!rs)
	{
		debug_printf("I2C::SendAddr %02X 发送读取地址失败\r\n", d);
		return false;
	}

	return rs;
}

bool I2C::SendSubAddr(int addr)
{
	//debug_printf("I2C::SendSubAddr addr=0x%02X \r\n", addr);

	// 发送子地址
	if (SubWidth > 0)
	{
		// 逐字节发送
		for (int k = SubWidth - 1; k >= 0; k--)
		{
			WriteByte(addr >> (k << 3));
			if (!WaitAck(true)) return false;
		}
	}

	return true;
}

// 新会话向指定地址写入多个字节
WEAK bool I2C::Write(int addr, const Buffer& bs)
{
	debug_printf("I2C::Write addr=0x%02X ", addr);
	bs.Show(true);

	if (!Open()) return false;

	I2CScope ics(this);

	bool rs = false;
	for (int i = 0; i < 5; i++) {
		// 发送设备地址
		rs = SendAddress(addr, true);
		if (rs) break;

		Stop();
		Sys.Sleep(1);
		Start();
	}
	if (!rs) return false;

	int len = bs.Length();
	for (int i = 0; i < len; i++)
	{
		WriteByte(bs[i]);
		// 最后一次不要等待Ack，因为有些设备返回Nak
		//if (i < len - 1 && !WaitAck(true)) return false;
		if (!WaitAck(i < len - 1)) return false;
		// EEPROM上最后一次也要等Ack，否则错乱
		//if (!WaitAck()) return false;
	}

	return true;
}

// 新会话从指定地址读取多个字节
WEAK uint I2C::Read(int addr, Buffer& bs)
{
	debug_printf("I2C::Read addr=0x%02X size=%d \r\n", addr, bs.Length());

	if (!Open()) return 0;

	I2CScope ics(this);

	bool rs = false;
	for (int i = 0; i < 5; i++) {
		// 发送设备地址
		rs = SendAddress(addr, false);
		if (rs) break;

		Stop();
		Sys.Sleep(1);
		Start();
	}
	if (!rs) return 0;

	uint count = 0;
	int len = bs.Length();
	for (int i = 0; i < len; i++)
	{
		bs[i] = ReadByte();
		count++;
		Ack(i < len - 1);	// 最后一次不需要发送Ack
	}
	return count;
}

bool I2C::Write(int addr, byte data) { return Write(addr, Buffer(&data, 1)); }

byte I2C::Read(int addr)
{
	ByteArray bs(1);
	if (!Read(addr, bs)) return 0;

	return bs[0];
}

ushort I2C::Read2(int addr)
{
	ByteArray bs(2);
	if (!Read(addr, bs)) return 0;

	// 小字节序
	return (bs[1] << 8) | bs[0];
}

uint I2C::Read4(int addr)
{
	ByteArray bs(4);
	if (!Read(addr, bs)) return 0;

	// 小字节序
	return (bs[3] << 24) | (bs[2] << 16) | (bs[1] << 8) | bs[0];
}

/*HardI2C::HardI2C(I2C_TypeDef* iic, uint speedHz ) : I2C()
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
}*/

HardI2C::HardI2C(byte index, uint speedHz) : I2C()
{
	Init(index, speedHz);
}

void HardI2C::Init(byte index, uint speedHz)
{
	_index = index;
	Speed = speedHz;

	OnInit();

	debug_printf("HardI2C_%d::Init %dHz \r\n", _index + 1, speedHz);
}

HardI2C::~HardI2C()
{
	Close();
}

void HardI2C::SetPin(Pin scl, Pin sda)
{
	SCL.Set(scl);
	SDA.Set(sda);
}

void HardI2C::GetPin(Pin* scl, Pin* sda)
{
	if (scl) *scl = SCL._Pin;
	if (sda) *sda = SDA._Pin;
}

/******************************** SoftI2C ********************************/

// 3种速率可选择  标准模式100kbps、快速模式400kbps、最高速率3.4Mbps
SoftI2C::SoftI2C(uint speedHz) : I2C()
{
	Speed = speedHz;
	_delay = Sys.Clock / speedHz;
	Retry = 100;
	Error = 0;
	Address = 0x00;
}

SoftI2C::~SoftI2C()
{
	Close();
}

void SoftI2C::SetPin(Pin scl, Pin sda)
{
	//SCL.Set(scl);
	//SDA.Set(sda);
	// 不用自动检测倒置。
	// 一般I2C初始都是高电平，也就是需要倒置
	// 但是为了更形象地表达高低电平，不要倒置
	SCL.Init(scl, false);
	SDA.Init(sda, false);
}

void SoftI2C::GetPin(Pin* scl, Pin* sda)
{
	if (scl) *scl = SCL._Pin;
	if (sda) *sda = SDA._Pin;
}

bool SoftI2C::OnOpen()
{
	assert(!SCL.Empty() && !SDA.Empty(), "未设置I2C引脚");

	debug_printf("I2C::Open Addr=0x%02X SubWidth=%d \r\n", Address, SubWidth);

	// 开漏输出
	SCL.OpenDrain = true;
	SDA.OpenDrain = true;

	debug_printf("\tSCL: ");
	SCL.Open();
	debug_printf("\tSDA: ");
	SDA.Open();

	// 当总线空闲时这两条线路都是高电平
	SCL = true;
	SDA = true;

	// SDA/SCL 默认上拉
	if (!SDA.ReadInput()) {
		debug_printf("打开失败，没有在SDA上检测到高电平！\r\n");

		SCL.Close();
		SDA.Close();

		return false;
	}

	return true;
}

void SoftI2C::OnClose()
{
	SCL.Close();
	SDA.Close();
}

void SoftI2C::Delay()
{
	//Sys.Delay(us);

	/*
	右移23位时：
	48M = 5;
	72M = 8;
	108M= 12;
	120M= 14;
	*/
	// 72M = 4
	int t = Sys.Clock >> 21;
	while (t-- > 0);
}

// 起始条件 当 SCL 处于高电平期间时，SDA 从高电平向低电平跳变时产生起始条件。
// 总线在起始条件产生后便处于忙的状态。起始条件常常简记为S。
/*
sda		   ----____
scl		___--------____
*/
void SoftI2C::Start()
{
	//debug_printf("SoftI2C::Start \r\n");
	// 在SCL高电平期间，SDA产生下降沿
	SCL = true;

	// 下降沿
	SDA = true;
	Delay();
	SDA = false;

	Delay();
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
	//debug_printf("SoftI2C::Stop \r\n");
	// 在SCL高电平期间，SDA产生上升沿
	SCL = false;
	SDA = false;
	Delay();

	SCL = true;
	Delay();
	SDA = true;
}

// 等待Ack
bool SoftI2C::WaitAck(bool ack)
{
	int retry = Retry;

	// SDA 线上的数据必须在时钟的高电平周期保持稳定
	SDA = true;
	SCL = true;
	Delay();

	if (ack) {
		// 等待SDA低电平
		while (SDA.ReadInput() == ack)
		{
			if (retry-- <= 0)
			{
				//debug_printf("SoftI2C::WaitAck Retry=%d 无法等到ACK \r\n", Retry);
				return false;
			}
		}
	}

	SCL = false;
	Delay();

	return true;
}

// 发送Ack
/*
在 I2C 总线传输数据过程中，每传输一个字节，都要跟一个应答状态位。
接收器接收数据的情况可以通过应答位来告知发送器。
应答位的时钟脉冲仍由主机产生，而应答位的数据状态则遵循“谁接收谁产生”的原则，即总是由接收器产生应答位。
主机向从机发送数据时，应答位由从机产生；主机从从机接收数据时，应答位由主机产生。

I2C 总线标准规定：
应答位为0 表示接收器应答（ACK），常常简记为A；为1 则表示非应答（NACK），常常简记为A。
发送器发送完LSB 之后，应当释放SDA 线（拉高SDA，输出晶体管截止），以等待接收器产生应答位。
如果接收器在接收完最后一个字节的数据，或者不能再接收更多的数据时，应当产 生非应答来通知发送器
*/
void SoftI2C::Ack(bool ack)
{
	// SDA 线上的数据必须在时钟的高电平周期保持稳定
	SCL = false;

	SDA = !ack;
	Delay();

	SCL = true;
	Delay();
	SCL = false;

	SDA = true;
	Delay();
}

void SoftI2C::WriteByte(byte dat)
{
	// SDA 线上的数据必须在时钟的高电平周期保持稳定
	SCL = false;
	for (byte mask = 0x80; mask > 0; mask >>= 1)
	{
		SDA = (dat & mask) > 0;
		Delay();

		// 置时钟线为高，通知被控器开始接收数据位
		SCL = true;
		Delay();
		SCL = false;
		Delay();
	}

	// 发送完一个字节数据后要主机要等待从机的应答，第九位
	SCL = false;	// 允许sda变化
	Delay();
	SDA = true;		// sda拉高等待应答，当sda=0时，表示从机的应答
	Delay();
}

byte SoftI2C::ReadByte()
{
	// SDA 线上的数据必须在时钟的高电平周期保持稳定
	SDA = true;
	byte rs = 0;
	for (byte mask = 0x80; mask > 0; mask >>= 1)
	{
		SCL = true;		// 置时钟线为高使数据线上数据有效
		//Delay(2);
		// 等SCL变高
		uint retry = 50;
		while (!SCL.ReadInput())
		{
			if (retry-- <= 0) break;
			Delay();
		}

		if (SDA.ReadInput()) rs |= mask;	//读数据位
		SCL = false;	// 置时钟线为低，准备接收数据位
		Delay();
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
