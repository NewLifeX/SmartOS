#include "Sys.h"
#include "Port.h"
#include "NRF24L01.h"

/********************************************************************************/
//NRF24L01寄存器操作命令
#define READ_REG_NRF    0x00  //读配置寄存器,低5位为寄存器地址
#define WRITE_REG_NRF   0x20  //写配置寄存器,低5位为寄存器地址
#define RD_RX_PLOAD     0x61  //读RX有效数据,1~32字节
#define WR_TX_PLOAD     0xA0  //写TX有效数据,1~32字节
#define FLUSH_TX        0xE1  //清除TX FIFO寄存器.发射模式下用
#define FLUSH_RX        0xE2  //清除RX FIFO寄存器.接收模式下用
#define REUSE_TX_PL     0xE3  //重新使用上一包数据,CE为高,数据包被不断发送.
#define NOP             0xFF  //空操作,可以用来读状态寄存器
/********************************************************************************/
//SPI(NRF24L01)寄存器地址
#define CONFIG          0x00  //配置收发状态，CRC校验模式及收发状态响应方式;
							  //bit0:1接收模式,0发射模式;bit1:电选择;bit2:CRC模式;bit3:CRC使能;
                              //bit4:中断MAX_RT(达到最大重发次数中断)使能;bit5:中断TX_DS使能;bit6:中断RX_DR使能
#define EN_AA           0x01  //自动应答功能设置  bit0~5,对应通道0~5
#define EN_RXADDR       0x02  //接收信道使能,bit0~5,对应通道0~5
#define SETUP_AW        0x03  //设置地址宽度(所有数据通道):bit[1:0]     00,3字节;01,4字节;02,5字节;
#define SETUP_RETR      0x04  //建立自动重发;bit3:0,自动重发计数器;bit7:4,自动重发延时 250*x+86us
#define RF_CH           0x05  //RF通道,bit6:0,工作通道频率;
#define RF_SETUP        0x06  //RF寄存器;bit3:传输速率(0:1Mbps,1:2Mbps);bit2:1,发射功率;bit0:低噪声放大器增益
#define STATUS          0x07  //状态寄存器;bit0:TX FIFO满标志;bit3:1,接收数据通道号(最大:6);
							  //bit4,达到最多次重发  bit5:数据发送完成中断;bit6:接收数据中断;
#define OBSERVE_TX      0x08  //发送检测寄存器,bit7:4,数据包丢失计数器;bit3:0,重发计数器
#define CD              0x09  //载波检测寄存器,bit0,载波检测;
#define RX_ADDR_P0      0x0A  //数据通道0接收地址,最大长度5个字节,低字节在前
#define RX_ADDR_P1      0x0B  //数据通道1接收地址,最大长度5个字节,低字节在前
#define RX_ADDR_P2      0x0C  //数据通道2接收地址,最低字节可设置,高字节,必须同RX_ADDR_P1[39:8]相等;
#define RX_ADDR_P3      0x0D  //数据通道3接收地址,最低字节可设置,高字节,必须同RX_ADDR_P1[39:8]相等;
#define RX_ADDR_P4      0x0E  //数据通道4接收地址,最低字节可设置,高字节,必须同RX_ADDR_P1[39:8]相等;
#define RX_ADDR_P5      0x0F  //数据通道5接收地址,最低字节可设置,高字节,必须同RX_ADDR_P1[39:8]相等;
#define TX_ADDR         0x10  //发送地址(低字节在前),ShockBurstTM模式下,RX_ADDR_P0与此地址相等
#define RX_PW_P0        0x11  //接收数据通道0有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P1        0x12  //接收数据通道1有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P2        0x13  //接收数据通道2有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P3        0x14  //接收数据通道3有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P4        0x15  //接收数据通道4有效数据宽度(1~32字节),设置为0则非法
#define RX_PW_P5        0x16  //接收数据通道5有效数据宽度(1~32字节),设置为0则非法
#define NRF_FIFO_STATUS 0x17  //FIFO状态寄存器;bit0,RX FIFO寄存器空标志;bit1,RX FIFO满标志;bit2,3,保留
                              //bit4,TX FIFO空标志;bit5,TX FIFO满标志;bit6,1,循环发送上一数据包.0,不循环;

NRF24L01::NRF24L01(Spi* spi, Pin ce, Pin irq)
{
    debug_printf("NRF24L01 CE=P%c%d IRQ=P%c%d\r\n", _PIN_NAME(ce), _PIN_NAME(irq));

	// 初始化地址
	//byte TX_ADDRESS[] = {0x34,0x43,0x10,0x10,0x01};
	//memcpy(Address, TX_ADDRESS, 5);
	memset(Address, 0, 5);
	memcpy(Address1, (byte*)Sys.ID, 5);
	for(int i=0; i<4; i++) Address2_5[i] = Address1[0] + i + 1;
	Channel = 0;	// 默认通道0

    if(ce != P0) _CE = new OutputPort(ce);
    if(irq != P0)
    {
        // 中断引脚初始化
        _IRQ = new InputPort(irq, false, InputPort::PuPd_UP);
		_IRQ->ShakeTime = 2;
        //_IRQ->Register(OnReceive, this);
    }
    // 必须先赋值，后面WriteReg需要用到
    _spi = spi;
	Timeout = 50;
	PayloadWidth = 32;
	AutoAnswer = true;
	Retry = 15;
	RetryPeriod = 500;	// 500us

	//_Received = NULL;
	//_Param = NULL;

    WriteReg(FLUSH_RX, NOP);   // 清除RX FIFO寄存器
	WriteReg(FLUSH_TX, NOP);   // 清除TX FIFO寄存器
}

NRF24L01::~NRF24L01()
{
    debug_printf("~NRF24L01\r\n");

	if(_CE) delete _CE;
	if(_IRQ) delete _IRQ;

	if(_spi) delete _spi;
	_spi = NULL;
	_CE = NULL;
	_IRQ = NULL;
}

// 向NRF的寄存器中写入一串数据
byte NRF24L01::WriteBuf(byte reg, const byte* buf, byte bytes)
{
	SpiScope sc(_spi);

	byte status = _spi->Write(WRITE_REG_NRF | reg);
	for(byte i=0; i<bytes; i++)
		_spi->Write(*buf++);

  	return status;
}

// 向NRF的寄存器中写入一串数据
byte NRF24L01::ReadBuf(byte reg, byte* buf, byte bytes)
{
	SpiScope sc(_spi);

	byte status = _spi->Write(reg);
	for(byte i=0; i<bytes; i++)
	  buf[i] = _spi->Write(NOP); //从NRF24L01读取数据

 	return status;
}

// 主要用于NRF与MCU是否正常连接
bool NRF24L01::Check(void)
{
	// 检查并打开Spi
	_spi->Open();

	//byte buf[5]={0xC2,0xC2,0xC2,0xC2,0xC2};
	byte buf[5]={0xA5,0xA5,0xA5,0xA5,0xA5};
	byte buf1[5];
	/*写入5个字节的地址.  */
	WriteBuf(TX_ADDR, buf, 5);
	/*读出写入的地址 */
	ReadBuf(TX_ADDR, buf1, 5);

	// 比较
	for(byte i=0; i<5; i++)
	{
		if(buf1[i] != buf[i]) return false; // 连接不正常
	}
	return true; // 成功连接
}

// 从NRF特定的寄存器读出数据 reg:NRF的命令+寄存器地址
byte NRF24L01::ReadReg(byte reg)
{
	SpiScope sc(_spi);

  	 /*发送寄存器号*/
	_spi->Write(reg);
	return _spi->Write(NOP);
}

// 向NRF特定的寄存器写入数据 NRF的命令+寄存器地址
byte NRF24L01::WriteReg(byte reg, byte dat)
{
	SpiScope sc(_spi);

	byte status = _spi->Write(WRITE_REG_NRF | reg);
    _spi->Write(dat);

   	return status;
}

// 配置
bool NRF24L01::Config()
{
#if DEBUG
	debug_printf("NRF24L01::Config\r\n");

	debug_printf("    Address: ");
	Sys.ShowHex(Address, 5, '-');
	debug_printf("\r\n");
	debug_printf("    Addres1: ");
	Sys.ShowHex(Address1, 5, '-');
	debug_printf("\r\n");
	for(int i=0; i<4; i++)
	{
		debug_printf("    Addres%d: ", i + 2);
		Sys.ShowHex(Address2_5 + i, 1, '-');
		debug_printf("-");
		Sys.ShowHex(Address1 + 1, 4, '-');
		debug_printf("\r\n");
	}
	debug_printf("    Channel: %d\r\n", Channel);
	debug_printf("    AutoAns: %d\r\n", AutoAnswer);
	debug_printf("    Payload: %d\r\n", PayloadWidth);
#endif

	CEDown();

	uint addrLen = ArrayLength(Address);

	WriteBuf(TX_ADDR, Address, addrLen);
	WriteBuf(RX_ADDR_P0, Address, addrLen); // 写接收端0地址
	WriteBuf(RX_ADDR_P1, Address1, addrLen); // 写接收端1地址
	// 写其它4个接收端的地址
	byte addr[ArrayLength(Address1)];
	memcpy(addr, Address1, addrLen);
	for(int i = 0; i < 4; i++)
	{
		WriteReg(RX_ADDR_P2 + i, Address2_5[i]);
	}

	// 使能6个接收端的自动应答和接收
	WriteReg(EN_AA, AutoAnswer ? 0x15 : 0);	// 使能通道0的自动应答
	WriteReg(EN_RXADDR, 0x15);		// 使能通道0的接收地址
	WriteReg(SETUP_AW, addrLen - 2); // 设置地址宽度

	//设置自动重发间隔时间:500us + 86us;最大自动重发次数:10次
	int period = RetryPeriod / 250 - 1;
	if(period < 0) period = 0;
	WriteReg(SETUP_RETR, (period << 4) | Retry);

	WriteReg(RF_CH, Channel); //设置RF通信频率
	WriteReg(RF_SETUP, 0x07); //设置TX发射参数,0db增益,1Mbps,低噪声增益开启

	// 设置6个接收端的数据宽度
	for(int i = 0; i < addrLen; i++)
	{
		WriteReg(RX_PW_P0 + i, PayloadWidth); // 选择通道0的有效数据宽度
	}

	// 编译器会优化下面的代码为一个常数
	RF_CONFIG config;
	config.Init();
	config.PWR_UP = 1;	// 1:上电 0:掉电
	config.CRCO = 1;	// CRC 模式‘0’-8 位CRC 校验‘1’-16 位CRC 校验
	config.EN_CRC = 1;	// CRC 使能如果EN_AA 中任意一位为高则EN_CRC 强迫为高
	//if(isReceive) config.PRIM_RX = 1;
	// 默认进入接收模式
	config.PRIM_RX = 1;
	
	/*config.MAX_RT = 1;
	config.TX_DS = 1;
	config.RX_DR = 1;*/
	
	byte mode = config.ToByte();
	WriteReg(CONFIG, mode);
	mode = ReadReg(CONFIG);
	//assert_param(mode == config.ToByte());
	// 如果电源还是关闭，则表示2401初始化失败
	if(!config.PWR_UP) return false;

	WriteReg(FLUSH_RX, NOP);	//清除RX FIFO寄存器
	WriteReg(FLUSH_TX, NOP);	//清除TX FIFO寄存器

	CEUp();
	// nRF24L01 在掉电模式下转入发射模式或接收模式前必须经过1.5ms 的待机模式
	Sys.Delay(1500);

	// 如果电源还是关闭，则表示2401初始化失败
	mode = ReadReg(CONFIG);
	config.Init(mode);
	return config.PWR_UP;
}

bool NRF24L01::SetMode(bool isReceive)
{
	byte mode = ReadReg(CONFIG);
	RF_CONFIG config;
	config.Init(mode);

	// 如果本来就是该模式，则不再处理
	if(!(isReceive ^ config.PRIM_RX)) return true;

	// 检查设置
	/*assert_param(config.PWR_UP);
	assert_param(config.CRCO);
	assert_param(config.EN_CRC);*/
	// 如果电源关闭，则重新打开电源
	if(!config.PWR_UP) config.PWR_UP = 1;

	if(isReceive) // 接收模式
	{
		config.PRIM_RX = 1;
        //WriteReg(FLUSH_RX, NOP);	//清除RX FIFO寄存器
	}
	else // 发送模式
	{
		config.PRIM_RX = 0;
        //WriteReg(FLUSH_TX, NOP);	//清除TX FIFO寄存器
	}
	// 必须拉低CE后修改配置，然后再拉高
	CEDown();
	WriteReg(CONFIG, config.ToByte());
	CEUp();

	// 如果电源还是关闭，则表示2401已经断开，准备重新初始化
	mode = ReadReg(CONFIG);
	config.Init(mode);
	if(!config.PWR_UP)
	{
		debug_printf("NRF24L01已经断开，准备重新初始化，当前配置Config=0x%02x\r\n", mode);
		Close();
		return false;
	}
	return true;
}

void NRF24L01::OnClose()
{
	byte mode = ReadReg(CONFIG);
	RF_CONFIG config;
	config.Init(mode);

	// 关闭电源
	config.PWR_UP = 0;

	CEDown();
	WriteReg(CONFIG, config.ToByte());
}

// 从NRF的接收缓冲区中读出数据
uint NRF24L01::OnRead(byte *data, uint len)
{
	// 如果不是异步，等待接收中断
	//if(!_Received && !WaitForIRQ()) return false;

	// 读取status寄存器的值
	Status = ReadReg(STATUS);
	if(Status == 0xFF) return 0;
	// 判断是否接收到数据
	RF_STATUS st;
	st.Init(Status);
	//if(!st.RX_DR || st.RX_P_NO == 0x07) return 0;
	if(!st.RX_DR) return 0;

	// 清除中断标志
	WriteReg(STATUS, Status);

	ReadBuf(RD_RX_PLOAD, data, PayloadWidth); // 读取数据
	WriteReg(FLUSH_RX, NOP);          // 清除RX FIFO寄存器

	return PayloadWidth;
}

// 向NRF的发送缓冲区中写入数据
bool NRF24L01::OnWrite(const byte* data, uint len)
{
	// 进入发送模式
	if(!SetMode(false)) return false;

	WriteBuf(WR_TX_PLOAD, data, PayloadWidth);

	bool rs = false;
	// 等待发送完成中断
	//if(!WaitForIRQ()) return false;
	// IRQ不可靠，改为轮询寄存器
	// 这里需要延迟一点时间，发送没有那么快完成
	ulong ticks = Time.NewTicks(Timeout * 1000);
	while(ticks > Time.CurrentTicks())
	{
		Status = ReadReg(STATUS);
		if(Status == 0xFF) return 0;
		RF_STATUS st;
		st.Init(Status);

		if(st.TX_DS || st.MAX_RT)
		{
			// 清除TX_DS或MAX_RT中断标志
			WriteReg(STATUS, Status);
			WriteReg(FLUSH_TX, NOP);    // 清除TX FIFO寄存器

			rs = st.TX_DS;
			break;
		}
	}

	SetMode(true);	// 发送完成以后进入接收模式

	return rs;
}

bool NRF24L01::WaitForIRQ()
{
	ulong ticks = Time.NewTicks(Timeout * 1000); // 等待100ms
	while(_IRQ->Read() && ticks > Time.CurrentTicks());
	if(ticks >= Time.CurrentTicks()) return true;

	// 读取状态寄存器的值
	Status = ReadReg(STATUS);
	if(Status == 0xFF) return false;
	debug_printf("NRF24L01::WaitForIRQ Timeout %dms\r\n", Timeout);

	return false;
}

void NRF24L01::CEUp()
{
    if(_CE)
	{
		*_CE = true;
		Sys.Delay(20); // 进入发送模式，高电平>10us
	}
}

void NRF24L01::CEDown()
{
    if(_CE) *_CE = false;
}

// 注册中断函数  不注册的结果是    有中断 无中断处理代码
/*void NRF24L01::Register(DataReceived handler, void* param)
{
    if(handler)
	{
        _Received = handler;
		_Param = param;

		//if(!_IRQ->Read()) OnReceive();
	}
    else
	{
        _Received = NULL;
		_Param = NULL;
	}
}

// 由引脚中断函数调用此函数  （nrf24l01的真正中断函数）
void NRF24L01::OnReceive()
{
	// 这里需要调整，检查是否有数据到来，如果有数据到来，则调用外部事件，让外部读取
	//if(down == false && _IRQ->Read() == false)  // 重新检测是否满足中断要求 低电平
	if(_Received) _Received(this, _Param);
}

// 引脚中断函数调用此函数  在NRF24L01::NRF24L01()构造函数中注册的是他  而不是上面一个
// typedef void (*IOReadHandler)(Pin pin, bool down, void* param);
void NRF24L01::OnReceive(Pin pin, bool down, void* param)
{
	debug_printf("IRQ down=%d\r\n", down);
	if(!down)
	{
		NRF24L01* nrf = (NRF24L01*)param;
		if(nrf) nrf->OnReceive();
	}
}*/

void NRF24L01::ShowStatus()
{
	RF_STATUS st;
	st.Init(Status);

	debug_printf("NRF24L01::Status=0x%02x ", Status);
	if(st.TX_FULL) debug_printf(" TX_FULL TX FIFO寄存器满标志");
	if(st.RX_P_NO != 6)
	{
		byte mode = ReadReg(CONFIG);
		RF_CONFIG config;
		config.Init(mode);
		// 只有接收模式才处理
		if(config.PRIM_RX)
		{
			if(st.RX_P_NO == 7)
				debug_printf(" RX_P_NO RX FIFO寄存器为空");
			else
				debug_printf(" RX_P_NO 接收数据通道号=%d", st.RX_P_NO);
		}
	}
	if(st.MAX_RT)
	{
		byte retr = ReadReg(SETUP_RETR);
		debug_printf(" MAX_RT 达到最多次重发次数%d 延迟%dus", retr & 0x0F, ((retr >> 4) + 1) * 250);
	}
	if(st.TX_DS) debug_printf(" TX_DS 数据发送完成中断");
	if(st.RX_DR) debug_printf(" RX_DR 接收数据中断");
	debug_printf("\r\n");
}

bool NRF24L01::CanReceive()
{
	// 读取状态寄存器的值
	Status = ReadReg(STATUS);
	if(Status == 0xFF) return false;

	RF_STATUS st;
	st.Init(Status);
	return st.RX_DR;
}
