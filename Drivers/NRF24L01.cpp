#include "Sys.h"
#include "Time.h"
#include "Port.h"
#include "NRF24L01.h"

#define RF_DEBUG DEBUG

#define RF2401_REG
#ifdef  RF2401_REG
/********************************************************************************/
// NRF24L01寄存器操作命令
#define READ_REG_NRF    0x00  //读配置寄存器,低5位为寄存器地址
#define WRITE_REG_NRF   0x20  //写配置寄存器,低5位为寄存器地址。只允许Shutdown、Standby、Idle-TX模式下操作
#define RD_RX_PLOAD     0x61  //读RX有效数据,1~32字节。接收模式中使用。从第0字节开始读，读完后数据从缓冲区中删除
#define WR_TX_PLOAD     0xA0  //写TX有效数据,1~32字节
#define FLUSH_TX        0xE1  //清除TX FIFO寄存器。发射模式下用
#define FLUSH_RX        0xE2  //清除RX FIFO寄存器。接收模式下用。如果在发送应答期间执行，数据应答将不能完成。
#define REUSE_TX_PL     0xE3  //使发送设备重复使用最后传输的数据。数据包将重复发送直到CE为低。
							  //TX PAYLOAD一直有效，直到执行W_TX_PAYLOAD或FLUSH_TX。
							  //在数据传输期间不能使能或禁止TX PAYLOAD REUSE功能。
#define NOP             0xFF  //空操作,可以用来读状态寄存器
// NRF24L01+有下面三个
#define ACTIVATE		0x50  //这个命令后跟随数据0x73激活以下功能：
							  //R_RX_PL_WID
							  //W_ACK_PAYLOAD
							  //W_TX_PAYLOAD_NOACK
							  //一个新的ACTIVATE命令跟随相同的数据将禁止这些功能。这个命令仅仅可以在掉电模式和省电模式下执行。
							  //R_RX_PL_WID,W_ACK_PAYLOAD和W_TX_PAYLOAD_NOACK功能寄存器初始状态是不可用的，写命令不起作用，读命令只能读到数据0
							  //（理解是数据可以写入寄存器，但是在没激活这项功能之前程序的设置不起作用）。
							  //用ACTIVATE命令后跟随数据0x73来使能这些寄存器。这个时候它们就可以像nRF24L01的其他寄存器那样访问了。
							  //再一次用相同的命令和数据可以禁止这些功能。
#define RX_PL_WID		0x60  //动态负载时，读取收到的数据字节数
#define ACK_PAYLOAD		0xA8  //适用于接收方，1010 1PPP 通过PIPE PPP将数据通过ACK的形式发出去，最多允许三帧数据存于FIFO中
#define TX_NOACK		0xB0  //适用于发射模式，使用这个命令需要将AUTOACK位置1
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
#define FIFO_STATUS 	0x17  //FIFO状态寄存器;bit0,RX FIFO寄存器空标志;bit1,RX FIFO满标志;bit2,3,保留
                              //bit4,TX FIFO空标志;bit5,TX FIFO满标志;bit6,1,循环发送上一数据包.0,不循环;
// NRF24L01+有下面
#define DYNPD			0x1C  //使能动态负载长度，6个字节表示6个通道的动态负载开关
#define FEATURE			0x1D  //特征寄存器

	class ByteStruct
	{
	public:
		void Init(byte data = 0) { *(byte*)this = data; }
		byte ToByte() { return *(byte*)this; }
	};

	// 配置寄存器0x00
	typedef struct : ByteStruct
	{
		byte PRIM_RX:1;	// 1:接收模式 0:发射模式。只能在Shutdown、Standby下更改
		byte PWR_UP:1;	// 1:上电 0:掉电
		byte CRCO:1;	// CRC 模式‘0’-8 位CRC 校验‘1’-16 位CRC 校验
		byte EN_CRC:1;	// CRC 使能如果EN_AA 中任意一位为高则EN_CRC 强迫为高
		byte MAX_RT:1;	// 可屏蔽中断MAX_RT, 1 IRQ 引脚不显示MAX_RT 中断, 0 MAX_RT 中断产生时IRQ 引脚电平为低
		byte TX_DS:1;	// 可屏蔽中断TX_DS, 1 IRQ 引脚不显示TX_DS 中断, 0 TX_DS 中断产生时IRQ 引脚电平为低
		byte RX_DR:1;	// 可屏蔽中断RX_RD, 1 IRQ 引脚不显示RX_RD 中断, 0 RX_RD 中断产生时IRQ 引脚电平为低
		byte Reserved:1;
	}RF_CONFIG;

	// 自动应答0x01
	typedef struct : ByteStruct
	{
		byte ENAA_P0:1;	// 数据通道0 自动应答允许
		byte ENAA_P1:1;	// 数据通道1 自动应答允许
		byte ENAA_P2:1;	// 数据通道2 自动应答允许
		byte ENAA_P3:1;	// 数据通道3 自动应答允许
		byte ENAA_P4:1;	// 数据通道4 自动应答允许
		byte ENAA_P5:1;	// 数据通道5 自动应答允许
		byte Reserved:2;
	}RF_EN_AA;

	// 接收地址允许0x02
	typedef struct : ByteStruct
	{
		byte ERX_P0:1;	// 接收数据通道0 允许
		byte ERX_P1:1;	// 接收数据通道1 允许
		byte ERX_P2:1;	// 接收数据通道2 允许
		byte ERX_P3:1;	// 接收数据通道3 允许
		byte ERX_P4:1;	// 接收数据通道4 允许
		byte ERX_P5:1;	// 接收数据通道5 允许
		byte Reserved:2;
	}RF_EN_RXADDR;

	// 设置地址宽度(所有数据通道)0x03
	typedef struct : ByteStruct
	{
		byte AW:2;	// 接收/发射地址宽度‘00’-无效‘01’-3 字节宽度‘10’-4 字节宽度‘11’-5 字节宽度
		byte Reserved:6;
	}RF_SETUP_AW;

	// 建立自动重发0x04
	typedef struct : ByteStruct
	{
		byte ARC:4;	// 自动重发计数‘0000’-禁止自动重发‘0000’-自动重发一次……‘0000’-自动重发15 次
		byte ARD:4;	// 自动重发延时‘0000’-等待250+86us‘0001’-等待500+86us‘0010’-等待750+86us……‘1111’-等待4000+86us
					// (延时时间是指一包数据发送完成到下一包数据开始发射之间的时间间隔)
	}RF_SETUP_RETR;

	// 射频通道0x05
	typedef struct : ByteStruct
	{
		byte CH:7;	// 设置nRF24L01 工作通道频率
		byte Reserved:1;
	}RF_CHANNEL;

	// 射频寄存器0x06
	typedef struct : ByteStruct
	{
		//byte LNA_HCURR:1;	// 低噪声放大器增益
		//byte POWER:2;		// 发射功率‘00’ -18dBm‘01’ -12dBm‘10’ -6dBm‘11’ 0dBm
		byte POWER:3;		// 台产版发射功率‘000’ -12dBm/-6/-4/0/1/3/4  '111' 7dBm
		byte DR:1;			// 数据传输率‘0’ –1Mbps ‘1’ 2 Mbps。台产版跟DR_HIGH配合
		byte PLL_LOCK:1;	// PLL_LOCK 允许仅应用于测试模式。台产必须为0
		byte DR_LOW:1;		// 射频数据率 [DR_LOW, DR]: 00：1Mbps 01：2Mbps 10：250kbps 11：保留
		byte Reserved:2;
	}ST_RF_SETUP;

	// 状态寄存器0x07
	typedef struct : ByteStruct
	{
		byte TX_FULL:1;	// TX FIFO 寄存器满标志, 1:TX FIFO 寄存器满, 0: TX FIFO 寄存器未满,有可用空间
		byte RX_P_NO:3;	// 接收数据通道号, 000-101:数据通道号, 110:未使用, 111:RX FIFO 寄存器为空
		byte MAX_RT:1;	// 达到最多次重发中断, 写‘1’清除中断, 如果MAX_RT 中断产生则必须清除后系统才能进行通讯
		byte TX_DS:1;	// 数据发送完成中断，当数据发送完成后产生中断。如果ACK模式下，只有收到ACK后此位置写‘1’清除中断
		byte RX_DR:1;	// 接收数据中断，当接收到有效数据后置一, 写‘1’清除中断
		byte Beken:1;	// 上海BK芯片特征
	}RF_STATUS;

	// 发送检测寄存器0x08
	typedef struct : ByteStruct
	{
		byte ARC_CNT:4;	// 重发计数器发送新数据包时此寄存器复位
		byte PLOS_CNT:4;// 数据包丢失计数器当写RF_CH 寄存器时此寄存器复位当丢失15个数据包后此寄存器重启
	}RF_OBSERVE_TX;

	// 载波检测0x09
	typedef struct : ByteStruct
	{
		byte CarrierDetection:1;	// 载波检测，接收信号强度RSSI，0表示信号<-60dBm
		byte Reserved:7;
	}RF_CD;

	// 射频通道0x17
	typedef struct : ByteStruct
	{
		byte RX_EMPTY:1;	// RX FIFO 寄存器空标志 1:RX FIFO 寄存器空 0: RX FIFO 寄存器非空
		byte RX_FULL:1;		// RX FIFO 寄存器满标志 1:RX FIFO 寄存器满 0: RX FIFO 寄存器未满有可用空间
		byte Reserved:2;
		byte TX_EMPTY:1;	// TX FIFO 寄存器空标志 1:TX FIFO 寄存器空 0: TX FIFO 寄存器非空
		byte TX_FULL:1;		// TX FIFO 寄存器满标志 1:TX FIFO 寄存器满 0: TX FIFO 寄存器未满有可用空间
		byte TX_REUSE:1;	// 若TX_REUSE=1 则当CE位高电平状态时不断发送上一数据包TX_REUSE 通过SPI 指令REUSE_TX_PL 设置通过W_TX_PALOAD或FLUSH_TX 复位
		byte Reserved2:1;
	}RF_FIFO_STATUS;

	// 特征寄存器0x1D
	typedef struct : ByteStruct
	{
		byte DYN_ACK:1;		// 使能命令TX_PAYLOAD_NOACK
		byte ACK_PAYD:1;	// 使能ACK负载（带负载数据的ACK包）
		byte DPL:1;			// 使能动态负载长度
		byte Reserved:5;
	}RF_FEATURE;
#endif

static void AutoOpenTask(void* param);

NRF24L01::NRF24L01()
{
	Power	= NULL;
	_spi	= NULL;

	// 初始化地址
	memset(Address, 0, ArrayLength(Address));
	memcpy(Address1, (byte*)Sys.ID, ArrayLength(Address1));
	for(int i=0; i<ArrayLength(Addr2_5); i++) Addr2_5[i] = Address1[0] + i + 1;
	Channel		= 0;	// 默认通道0
	AddrBits	= 0x01;	// 默认使能地址0

	Timeout		= 50;
	PayloadWidth= 32;
	AutoAnswer	= true;
	Retry		= 15;
	RetryPeriod	= 500;	// 500us
	Speed		= 250;
	RadioPower	= 0xFF;

	MaxError	= 10;
	Error		= 0;

	_tidOpen	= 0;
	_tidRecv	= 0;

	_Lock	= 0;

	LedTx	= NULL;
	LedRx	= NULL;
}

void NRF24L01::Init(Spi* spi, Pin ce, Pin irq, Pin power)
{
	debug_printf("NRF24L01::Init CE=P%c%d IRQ=P%c%d Power=P%c%d\r\n", _PIN_NAME(ce), _PIN_NAME(irq), _PIN_NAME(power));

    if(ce != P0)
	{
		// 拉高CE，可以由待机模式切换到RX/TX模式
		// 拉低CE，由收发模式切换回来待机模式
		_CE.OpenDrain = false;
		_CE.Set(ce);
		//_CE = false;	// 开始让CE=0，系统上电并打开电源寄存器后，位于待机模式
	}
    if(irq != P0)
    {
        // 中断引脚初始化
		//_IRQ->ShakeTime = 2;
		_IRQ.Floating	= false;
		_IRQ.PuPd		= InputPort::PuPd_UP;
		_IRQ.HardEvent	= true;
		_IRQ.Set(irq);
        _IRQ.Register(OnIRQ, this);
    }
	if(power != P0)
	{
		Power.Set(power);
	}

    // 必须先赋值，后面WriteReg需要用到
    _spi = spi;

	/*// 需要先打开SPI，否则后面可能不能及时得到读数
	_spi->Open();
#if !DEBUG
	TimeWheel tw(0, 100);
	while(!GetPower() && !tw.Expired()) Sys.Sleep(1);
#endif

	// 初始化前必须先关闭电源。因为系统可能是重启，而模块并没有重启，还保留着上一次的参数
	//!!! 重大突破！当前版本程序，烧写后无法触发IRQ中断，但是重新上电以后可以中断，而Reset也不能触发。并且发现，只要模块带电，寄存器参数不会改变。
	SetPower(false);*/
}

NRF24L01::~NRF24L01()
{
    debug_printf("NRF24L01::~NRF24L01\r\n");

	Sys.RemoveTask(_tidOpen);
	Sys.RemoveTask(_tidRecv);

	Register(NULL);

	// 关闭电源
	SetPower(false);

	delete _spi;
	_spi = NULL;

	delete LedTx;
	LedTx = NULL;

	delete LedRx;
	LedRx = NULL;
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
#if RF_DEBUG
	/*if(!GetMode())
		debug_printf("NRF24L01::ReadBuf 只能接收模式下用。\r\n");*/
#endif
	SpiScope sc(_spi);

	byte status = _spi->Write(reg);
	for(byte i=0; i<bytes; i++)
		buf[i] = _spi->Write(NOP); //从NRF24L01读取数据

 	return status;
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
#if RF_DEBUG
	/*if(_CE && _CE->Read())
	{
		debug_printf("NRF24L01::WriteReg 只允许Shutdown、Standby、Idle-TX模式下操作\r\n");
	}*/
#endif
	SpiScope sc(_spi);

	byte status = _spi->Write(WRITE_REG_NRF | reg);
    _spi->Write(dat);

   	return status;
}

// 主要用于NRF与MCU是否正常连接
bool NRF24L01::Check(void)
{
	if(!Open()) return false;

	//byte buf[5]={0xC2,0xC2,0xC2,0xC2,0xC2};
	byte buf[5]={0xA5,0xA5,0xA5,0xA5,0xA5};
	byte buf1[5];
	byte buf2[5];

	// 必须拉低CE后修改配置，然后再拉高
	PortScope ps(&_CE, false);

	//!!! 必须确保还原回原来的地址，否则会干扰系统的正常工作
	// 读出旧有的地址
	ReadBuf(TX_ADDR, buf1, 5);

	// 写入新的地址
	WriteBuf(TX_ADDR, buf, 5);
	// 读出写入的地址
	ReadBuf(TX_ADDR, buf2, 5);

	// 写入旧有的地址
	WriteBuf(TX_ADDR, buf1, 5);

	// 比较
	for(byte i=0; i<5; i++)
	{
		if(buf2[i] != buf[i]) return false; // 连接不正常
	}

	return true; // 成功连接
}

// 配置
bool NRF24L01::Config()
{
#if RF_DEBUG
	debug_printf("NRF24L01::Config\r\n");

	// 检查芯片特征
	Status = ReadReg(STATUS);
	RF_STATUS st;
	st.Init(Status);
	if(st.Beken) debug_printf("上海Beken芯片\r\n");

	debug_printf("    本地地址: ");
	Sys.ShowHex(Address, 5, '-');
	debug_printf("\r\n");

	// 根据AddrBits决定相应通道是否打开
	byte bits = AddrBits >> 1;
	if(bits & 0x01)
	{
		debug_printf("    Addres1: ");
		Sys.ShowHex(Address1, 5, '-');
		debug_printf("\r\n");
	}
	for(int i=0; i<4; i++)
	{
		bits >>= 1;
		if(bits & 0x01)
		{
			debug_printf("    Addres%d: ", i + 2);
			Sys.ShowHex(Addr2_5 + i, 1, '-');
			debug_printf("-");
			Sys.ShowHex(Address1 + 1, 4, '-');
			debug_printf("\r\n");
		}
	}
	static const short powers[] = {-12, -6, -4, 0, 1, 3, 4, 7};
	if(RadioPower >= ArrayLength(powers)) RadioPower = ArrayLength(powers) - 1;
	debug_printf("    射频通道: %d  %dMHz %ddBm\r\n", Channel, 2400 + Channel, powers[RadioPower]);
	// 检查WiFi通道
	static const byte wifis[] = {2, 32, 70, 5, 35, 68, 8, 39, 65, 11, 41, 62};
	for(int i=0; i<ArrayLength(wifis); i++)
	{
		if(wifis[i] == Channel)
		{
			debug_printf("通道 %d 与WiFi通道 %d 的频率相同，可能有干扰！\r\n", Channel, i);
			break;
		}
	}
	debug_printf("    传输速率: ");
	if(Speed >= 1000)
		debug_printf("%dMbps\r\n", Speed/1000);
	else
		debug_printf("%dkbps\r\n", Speed);
	debug_printf("    负载大小: %d字节\r\n", PayloadWidth);
	debug_printf("    自动应答: %s\r\n", AutoAnswer ? "true" : "false");
	if(AutoAnswer)
	{
		debug_printf("    应答重试: %d次\r\n", Retry);
		int period = RetryPeriod / 250 - 1;
		if(period < 0) period = 0;
		RetryPeriod = (period + 1) * 250;
		debug_printf("    重试周期: %dus + 86us\r\n", RetryPeriod);
	}
	debug_printf("    发送超时: %dms\r\n", Timeout);
	debug_printf("    出错重启: %d次  发送失败等出错超过%d次将会重启模块\r\n", MaxError, MaxError);
#endif

	//ShowStatus();
	SetPower(false);
	_CE = false;

	SetAddress(true);

	if(AutoAnswer)
	{
		//设置自动重发间隔时间:500us + 86us;最大自动重发次数:10次
		int period = RetryPeriod / 250 - 1;
		if(period < 0) period = 0;
		//WriteReg(SETUP_RETR, (period << 4) | Retry);

		RF_SETUP_RETR retr;
		retr.ARC = Retry;
		retr.ARD = period;
		WriteReg(SETUP_RETR, retr.ToByte());
	}
	else
	{
		WriteReg(SETUP_RETR, 0);
	}

	WriteReg(RF_CH, Channel);					// 设置RF通信频率

	ST_RF_SETUP set;
	set.Init();
	set.POWER = RadioPower;
	switch(Speed)
	{
		case 250: {set.DR = 0; set.DR_LOW = 1; break;}
		case 1000: {set.DR = 0; set.DR_LOW = 0; break;}
		case 2000: {set.DR = 1; set.DR_LOW = 0; break;}
	}

	WriteReg(RF_SETUP, set.ToByte());

	// 编译器会优化下面的代码为一个常数
	RF_CONFIG config;
	config.Init();
	//config.PRIM_RX	= 0;
	config.PWR_UP	= 0;	// 1:上电 0:掉电
	config.CRCO		= 1;	// CRC 模式‘0’-8 位CRC 校验‘1’-16 位CRC 校验
	config.EN_CRC	= 1;	// CRC 使能如果EN_AA 中任意一位为高则EN_CRC 强迫为高
	config.PRIM_RX	= 1;	// 默认进入接收模式

	config.MAX_RT	= 0;
	config.TX_DS	= 0;
	config.RX_DR	= 0;

	byte mode = config.ToByte();
	WriteReg(CONFIG, mode);

	// 在ACK模式下发送失败和接收失败要清空发送缓冲区和接收缓冲区，否则不能进行下次发射或接收
	ClearFIFO(true);
	ClearFIFO(false);
	// 清除中断标志
	ClearStatus(true, true);

	if(!SetPower(true)) return false;

	_CE = true;

	//Sys.Sleep(1000);
	// 上电后并不能马上使用，需要一段时间才能载波。标称100ms，实测1000ms
	//debug_printf("    载波检测: %s\r\n", ReadReg(CD) > 0 ? "通过" : "失败");
	if(ReadReg(CD) > 0) debug_printf("空间发现载波，可能存在干扰或者拥挤！\r\n");
	//ShowStatus();

	return true;
}

/*bool NRF24L01::CheckConfig()
{
	for(int i=0; i<0x20; i++)
	{
		byte dat = ReadReg(i);
		debug_printf("Reg 0x%02x = 0x%02x\r\n", i, dat);
	}

	return true;
}*/

void NRF24L01::ClearFIFO(bool rx)
{
#if RF_DEBUG
	if(rx)
	{
		if(!GetMode()) debug_printf("NRF24L01::ClearFIFO 清空RX缓冲区只能接收模式下用。如果在发送应答期间执行，数据应答将不能完成。\r\n");
	}
	else
	{
		if(GetMode()) debug_printf("NRF24L01::ClearFIFO 清空TX缓冲区只能发送模式下用。\r\n");
	}
#endif
	if(rx)
		WriteReg(FLUSH_RX, NOP);
	else
		WriteReg(FLUSH_TX, NOP);
}

void NRF24L01::ClearStatus(bool tx, bool rx)
{
	// 发送标识位 TX_DS/MAX_RT
	if(tx) WriteReg(STATUS, 0x30);
	// 接收标识位 RX_DR
	if(rx) WriteReg(STATUS, 0x40);
}

// 获取当前电源状态
bool NRF24L01::GetPower()
{
	byte mode = ReadReg(CONFIG);
	RF_CONFIG config;
	config.Init(mode);

	return config.PWR_UP;
}

// 获取/设置当前电源状态
bool NRF24L01::SetPower(bool on)
{
	byte mode = ReadReg(CONFIG);
	RF_CONFIG config;
	config.Init(mode);

	if(!(on ^ config.PWR_UP)) return true;

	debug_printf("NRF24L01::SetPower %s电源\r\n", on ? "打开" : "关闭");

	config.PWR_UP = on ? 1 : 0;

	{
		// 故意缩进，为了让ps提前析构，拉高CE
		//PortScope ps(_CE, false);
		WriteReg(CONFIG, config.ToByte());
	}
	if(on)
	{
		// 如果是上电，需要等待1.5~2ms
		Sys.Delay(2000);

		// 再次检查是否打开了电源
		config.Init(ReadReg(CONFIG));
		if(!config.PWR_UP)
		{
			debug_printf("NRF24L01::SetPower 无法打开电源！\r\n");
			return false;
		}
	}

	return true;
}

// 获取当前模式是否接收模式
bool NRF24L01::GetMode()
{
	byte mode = ReadReg(CONFIG);
	RF_CONFIG config;
	config.Init(mode);

	return config.PRIM_RX;
}

// 设置收发模式。
// 因为CE拉高需要延迟的原因，整个函数大概耗时185us，如果不延迟，大概55us
bool NRF24L01::SetMode(bool isReceive)
{
	byte mode = ReadReg(CONFIG);
	RF_CONFIG config;
	config.Init(mode);

	// 如果本来就是该模式，则不再处理
	if(!(isReceive ^ config.PRIM_RX)) return true;

	// 如果电源关闭，则重新打开电源
	if(!config.PWR_UP) config.PWR_UP = 1;

	// 必须拉低CE后修改配置，然后再拉高
	_CE = false;

	// 不能随便清空FIFO寄存器，否则收到的数据都被清掉了
	if(isReceive) // 接收模式
	{
		config.PRIM_RX = 1;
		ClearStatus(false, true);
	}
	else // 发送模式
	{
		config.PRIM_RX = 0;
		ClearStatus(true, false);
	}
	WriteReg(CONFIG, config.ToByte());

	if(isReceive)
	{
		// 进入接收模式，如果此时接收缓冲区已满，则清空缓冲区。否则无法收到中断
		RF_FIFO_STATUS fifo;
		fifo.Init(FifoStatus);
		if(fifo.RX_FULL)
		{
			//debug_printf("RX缓冲区满，需要清空！\r\n");
			ClearFIFO(true);
		}
	}
	else
	{
		// 发送前清空缓冲区和标识位
		ClearFIFO(false);
	}
	_CE = true;

	// 进入发射模式等一会
	if(!isReceive) Sys.Delay(200);

	// 如果电源还是关闭，则表示2401已经断开，准备重新初始化
	mode = ReadReg(CONFIG);
	config.Init(mode);
	if(mode == 0xFF || !config.PWR_UP)
	{
		debug_printf("NRF24L01已经断开，准备重新初始化，当前配置Config=0x%02x\r\n", mode);
		Close();
		if(Open()) return true;

		if(_tidOpen)
			Sys.SetTask(_tidOpen, true);
		else
			_tidOpen = Sys.AddTask(AutoOpenTask, this, 5000000, 5000000, "R24热插拔");
		return false;
	}

	return true;
}

// 设置地址。参数指定是否设置0通道地址以外的完整地址
void NRF24L01::SetAddress(bool full)
{
	uint addrLen = ArrayLength(Address);

	WriteBuf(TX_ADDR, Address, addrLen);
	WriteBuf(RX_ADDR_P0, Address, addrLen);		// 写接收端0地址
	WriteReg(SETUP_AW, addrLen - 2);			// 设置地址宽度

	if(!full) return;

	byte bits = AddrBits >> 1;
	if(bits & 0x01)
	{
		WriteBuf(RX_ADDR_P1, Address1, addrLen);	// 写接收端1地址
	}
	// 写其它4个接收端的地址
	byte addr[ArrayLength(Address1)];
	memcpy(addr, Address1, addrLen);
	for(int i = 0; i < 4; i++)
	{
		bits >>= 1;
		if(bits & 0x01)
		{
			addr[0] = Addr2_5[i];
			WriteBuf(RX_ADDR_P2 + i, addr, addrLen);
		}
	}

	// 使能6个接收端的自动应答和接收
	WriteReg(EN_AA, AutoAnswer ? AddrBits : 0);	// 使能通道0的自动应答
	WriteReg(EN_RXADDR, AddrBits);				// 使能通道0的接收地址

	// 开启隐藏寄存器
	WriteReg(ACTIVATE, 0x73);
#if RF_DEBUG
	//debug_printf("R24::ACTIVATE\t= 0x%02X\r\n", ReadReg(ACTIVATE));
#endif

	RF_FEATURE ft;
	ft.Init(ReadReg(FEATURE));

	// 设置6个接收端的数据宽度
	if(PayloadWidth > 0)
	{
		for(int i = 0; i < addrLen; i++)
		{
			WriteReg(RX_PW_P0 + i, PayloadWidth);	// 选择通道0的有效数据宽度
		}
	}
	else
	{
		// 动态负载
		WriteReg(DYNPD, 0x3F);	// 打开6个通道的动态负载
#if RF_DEBUG
		//debug_printf("R24::DYNPD\t= 0x%02X\r\n", ReadReg(DYNPD));
#endif

		ft.DPL = 1;			// 使能动态负载长度
	}

	if(!AutoAnswer)
		ft.DYN_ACK = 1;	// 使能命令TX_PAYLOAD_NOACK
	//ft.ACK_PAYD = 1;	// 使能ACK负载（带负载数据的ACK包）

	WriteReg(FEATURE, ft.ToByte());
#if RF_DEBUG
	//debug_printf("R24::FEATURE\t= 0x%02X => 0x%02X\r\n", ft.ToByte(), ReadReg(FEATURE));
#endif
}

void ShowStatusTask(void* param)
{
	NRF24L01* nrf = (NRF24L01*)param;

	debug_printf("定时 ");
	nrf->ShowStatus();
	//nrf->CheckConfig();

	// 可以定时做一些事情，方便调试
	//nrf->ClearStatus(false, true);
	//nrf->ClearFIFO(true);
}

void AutoOpenTask(void* param)
{
	assert_ptr(param);

	NRF24L01* nrf = (NRF24L01*)param;
	nrf->Open();
}

bool NRF24L01::OnOpen()
{
	if(Power.Open() && !Power.Read())
	{
		Power = true;
		debug_printf("NRF24L01::打开 物理电源开关\r\n");
	}
	_CE.Open();
	_IRQ.Open();
	// 检查并打开Spi
	_spi->Open();

	Error = 0;

	// 配置完成以后，无需再次检查
	if(!Check())
	{
		debug_printf("查找模块失败，请检查是否已安装或者通信线路是否畅通\r\n");
		_spi->Close();
		return false;
	}
	if(!Config())
	{
		_spi->Close();
		return false;
	}

	// 打开成功后，关闭定时轮询任务
	if(_tidOpen)
	{
		debug_printf("NRF24L01::关闭 2401热插拔检查 ");
		Sys.SetTask(_tidOpen, false);
	}
	if(!_tidRecv)
		_tidRecv = Sys.AddTask(ReceiveTask, this, 200000, 200000, "R24接收");
	else
		Sys.SetTask(_tidRecv, true);

	//debug_printf("定时显示状态 ");
	//Sys.AddTask(ShowStatusTask, this, 5000000, 5000000);

	return true;
}

void NRF24L01::OnClose()
{
	if(_tidRecv) Sys.SetTask(_tidRecv, false);

	SetPower(false);

	_spi->Close();
	_CE.Close();
	_IRQ.Close();
	if(Power.Read())
	{
		Power = false;
		debug_printf("NRF24L01::关闭 物理电源开关\r\n");
	}
	Power.Close();
}

// 从NRF的接收缓冲区中读出数据
uint NRF24L01::OnRead(byte *data, uint len)
{
	// 亮灯。离开时自动熄灯
	//PortScope ps(LedRx);

	Lock lock(_Lock);
	//if(!lock.Wait(10000)) return false;
	// 如果拿不到锁，马上退出，不等待。因为一般读取是由定时器中断来驱动，等待没有意义。
	if(!lock.Success) return false;

	// 进入接收模式
	if(!SetMode(true)) return false;

	uint rs = 0;
	// 读取status寄存器的值
	Status = ReadReg(STATUS);
	if(Status == 0xFF)
	{
		AddError();
	}
	else
	{
		// 判断是否接收到数据
		RF_STATUS st;
		st.Init(Status);
		//if(!st.RX_DR || st.RX_P_NO == 0x07) return 0;
		// 单个2401模块独立工作时，也会收到乱七八糟的数据，通过判断RX_P_NO可以过滤掉一部分
		if(st.RX_DR)
		{
			// 清除中断标志
			//WriteReg(STATUS, Status);

			if(PayloadWidth > 0)
				rs = PayloadWidth;
			else
				rs = ReadReg(RX_PL_WID);

			if(rs > len)
			{
				debug_printf("NRF24L01::Read 实际负载%d，缓冲区大小%d，为了稳定，使用缓冲区大小\r\n", rs, len);
				rs = len;
			}
			ReadBuf(RD_RX_PLOAD, data, rs); // 读取数据
		}
	}

	// 进入Standby
	//_CE = false;

	// 清除中断标志
	ClearStatus(false, true);
	ClearFIFO(true);

	//_CE = true;
	if(rs && LedRx) *LedRx = !*LedRx;

	return rs;
}

// 向NRF的发送缓冲区中写入数据
bool NRF24L01::OnWrite(const byte* data, uint len)
{
	// 亮灯。离开时自动熄灯
	//PortScope ps(LedTx);
	if(LedTx) *LedTx = !*LedTx;

	Lock lock(_Lock);
	if(!lock.Wait(10000)) return false;

	/*// 检查空间载波，避开
	int retry = 2000;
	while(ReadReg(CD))
	{
		if(retry-- <= 0)
		{
			debug_printf("空间载波拥挤，发送失败！\r\n");
			return false;
		}
		Sys.Delay(50);	// 暂停50微秒
	}*/

	// 进入发送模式
	if(!SetMode(false)) return false;

	// 进入Standby，写完数据再进入TX发送。这里开始直到CE拉高之后，共耗时176us。不拉高CE大概45us
	_CE = false;

	byte cmd = AutoAnswer ? WR_TX_PLOAD : TX_NOACK;
	// 检查要发送数据的长度
	assert_param(PayloadWidth == 0 || len <= PayloadWidth);
	if(PayloadWidth > 0) len = PayloadWidth;
	WriteBuf(cmd, data, len);

	// 进入TX，维持一段时间
	_CE = true;

	bool rs = false;
	// 这里需要延迟一点时间，发送没有那么快完成。整个循环大概耗时20us
	//TimeWheel tw(0, Timeout);
	//TimeWheel tw(0, RetryPeriod * Retry);

	uint ms = RetryPeriod * Retry;
	if(ms > 4 && AutoAnswer) ms = 4;
	// https://devzone.nordicsemi.com/question/17074/nrf24l01-data-loss/
	// It is important never to keep the nRF24L01+ in TX mode for more than 4ms at a time.
	// If the Enhanced ShockBurst™ features are enabled, nRF24L01+ is never in TX mode longer than 4ms
	TimeWheel tw(0, ms);
	do
	{
		Status = ReadReg(STATUS);
		if(Status == 0xFF)
		{
			AddError();
			// 这里不能直接跳出函数，即使发送失败，也要在后面进入接收模式
			break;
		}

		RF_STATUS st;
		st.Init(Status);

		if(st.TX_DS || st.MAX_RT)
		{
			_CE = false;
			// 清除TX_DS或MAX_RT中断标志
			WriteReg(STATUS, Status);

			rs = st.TX_DS;

			if(!st.TX_DS && st.MAX_RT)
			{
#if RF_DEBUG
				if(st.MAX_RT)
				{
					//debug_printf("发送超过最大重试次数\r\n");
					byte retr = ReadReg(SETUP_RETR);
					debug_printf(" MAX_RT 达到最大重发次数%d 延迟%dus\r\n", retr & 0x0F, ((retr >> 4) + 1) * 250);
				}
#endif
				AddError();
			}

			break;
		}
	}while(!tw.Expired());

	// 这里开始到CE拉高结束，大概耗时186us。不拉高CE大概20us
	_CE = false;
	ClearFIFO(false);

	SetMode(true);	// 发送完成以后进入接收模式

	return rs;
}

void NRF24L01::AddError()
{
	Error++;
	if(MaxError > 0 && Error >= MaxError)
	{
		debug_printf("RF24::Error 出错%d次，超过最大次数%d，准备重启模块\r\n", Error, MaxError);

		Close();
		Open();
	}
}

void NRF24L01::OnIRQ(Pin pin, bool down, void* param)
{
	// 必须在down=true才能读取到正确的状态
	if(!down) return;

	NRF24L01* nrf = (NRF24L01*)param;
	if(!nrf) return;

	// 马上调度任务
	Sys.SetTask(nrf->_tidRecv, true, 0);
}

void NRF24L01::OnIRQ()
{
	// 读状态寄存器
	Status = ReadReg(STATUS);
	FifoStatus = ReadReg(FIFO_STATUS);

	RF_STATUS st;
	st.Init(Status);
	RF_FIFO_STATUS fifo;
	fifo.Init(FifoStatus);

	// 发送完成或最大重试次数以后进入接收模式
	if(st.TX_DS || st.MAX_RT)
	{
		// 达到最大重试次数以后，需要清空TX缓冲区
		if(st.MAX_RT) ClearFIFO(false);
		//SetMode(true);
	}

	// TX_FIFO 缓冲区满
	if(fifo.TX_FULL)
	{
		PortScope ps(&_CE, false);
		ClearFIFO(false);
		ClearStatus(true, false);
		//SetMode(false);
	}

	if(st.RX_DR)
	{
		byte buf[64];
		uint len = Read(buf, ArrayLength(buf));
		//ClearStatus(false, true);
		if(len)
		{
			len = OnReceive(buf, len);

			// 如果有返回，说明有数据要回复出去
			if(len) Write(buf, len);
		}
		return;
	}
	else
	{
		// RX_FIFO 缓冲区满
		if(fifo.RX_FULL)
		{
			PortScope ps(&_CE, false);
			ClearFIFO(true);
			ClearStatus(false, true);
			//SetMode(true);
		}
	}
}

void NRF24L01::ShowStatus()
{
#if RF_DEBUG
	// 读状态寄存器
	Status = ReadReg(STATUS);
	FifoStatus = ReadReg(FIFO_STATUS);
#endif

	RF_STATUS st;
	st.Init(Status);

	debug_printf("Status=0x%02x ", Status);
	if(st.TX_FULL) debug_printf(" TX_FULL TX FIFO寄存器满标志");
	if(st.RX_P_NO != 6)
	{
		// 只有接收模式才处理
		if(GetMode())
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
		debug_printf(" MAX_RT 达到最大重发次数%d 延迟%dus", retr & 0x0F, ((retr >> 4) + 1) * 250);
	}
	if(st.TX_DS) debug_printf(" TX_DS 发送完成");
	if(st.RX_DR) debug_printf(" RX_DR 接收数据");
	//debug_printf("\r\n");
	debug_printf("\t");

	RF_FIFO_STATUS fifo;
	fifo.Init(FifoStatus);

	debug_printf("FIFO_STATUS=0x%02x ", *(byte*)&fifo);
	if(fifo.RX_EMPTY) debug_printf(" RX FIFO 寄存器空");
	if(fifo.RX_FULL)  debug_printf(" RX FIFO 寄存器满");
	if(fifo.TX_EMPTY) debug_printf(" TX FIFO 寄存器空");
	if(fifo.TX_FULL)  debug_printf(" TX FIFO 寄存器满");
	if(fifo.TX_REUSE) debug_printf(" 当CE位高电平状态时不断发送上一数据包");
	debug_printf("\r\n");
}

void NRF24L01::ReceiveTask(void* param)
{
	assert_ptr(param);

	NRF24L01* nrf = (NRF24L01*)param;
	// 需要判断锁，如果有别的线程正在读写，则定时器无条件退出。
	if(nrf->Opened && nrf->_Lock == 0)
	{
		nrf->OnIRQ();
	}
}
