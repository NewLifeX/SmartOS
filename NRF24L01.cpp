#include "Sys.h"
#include "Port.h"
#include "NRF24L01.h"

#define RF2401_REG
#ifdef  RF2401_REG
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
#define FIFO_STATUS 	0x17  //FIFO状态寄存器;bit0,RX FIFO寄存器空标志;bit1,RX FIFO满标志;bit2,3,保留
                              //bit4,TX FIFO空标志;bit5,TX FIFO满标志;bit6,1,循环发送上一数据包.0,不循环;
#endif

NRF24L01::NRF24L01(Spi* spi, Pin ce, Pin irq)
{
    debug_printf("NRF24L01 CE=P%c%d IRQ=P%c%d\r\n", _PIN_NAME(ce), _PIN_NAME(irq));

	// 初始化地址
	memset(Address, 0, 5);
	memcpy(Address1, (byte*)Sys.ID, 5);
	for(int i=0; i<4; i++) Address2_5[i] = Address1[0] + i + 1;
	Channel = 0;	// 默认通道0

	_CE = NULL;
    if(ce != P0) _CE = new OutputPort(ce, true, false);
	_IRQ = NULL;
    if(irq != P0)
    {
        // 中断引脚初始化
        _IRQ = new InputPort(irq, false, InputPort::PuPd_UP);
		_IRQ->ShakeTime = 2;
        _IRQ->Register(OnIRQ, this);
    }
    // 必须先赋值，后面WriteReg需要用到
    _spi = spi;
	Timeout = 50;
	PayloadWidth = 32;
	AutoAnswer = true;
	Retry = 15;
	RetryPeriod = 500;	// 500us

	MaxError = 10;
	Error = 0;

	// 这里不用急着清空寄存器，Config的时候会清空
    //WriteReg(FLUSH_RX, NOP);   // 清除RX FIFO寄存器
	//WriteReg(FLUSH_TX, NOP);   // 清除TX FIFO寄存器

	//_taskID = 0;
	//_timer = NULL;
	//_Thread = NULL;

	_Lock = 0;
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

	//if(_taskID) Sys.RemoveTask(_taskID);
	Register(NULL);
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

// 主要用于NRF与MCU是否正常连接
bool NRF24L01::Check(void)
{
	// 检查并打开Spi
	_spi->Open();

	//byte buf[5]={0xC2,0xC2,0xC2,0xC2,0xC2};
	byte buf[5]={0xA5,0xA5,0xA5,0xA5,0xA5};
	byte buf1[5];
	byte buf2[5];

	// 必须拉低CE后修改配置，然后再拉高
	PortScope ps(_CE);

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

	PortScope ps(_CE);

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
	//WriteReg(RF_SETUP, 0x07); //设置TX发射参数,0db增益,1Mbps,低噪声增益开启
	WriteReg(RF_SETUP, 0x2F); //设置TX发射参数,7db增益,2Mbps,低噪声增益开启

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
	// 默认进入接收模式
	config.PRIM_RX = 1;

	config.MAX_RT = 1;
	config.TX_DS = 1;
	/*config.RX_DR = 1;*/

	byte mode = config.ToByte();
	WriteReg(CONFIG, mode);
	mode = ReadReg(CONFIG);
	//assert_param(mode == config.ToByte());
	// 如果电源还是关闭，则表示2401初始化失败
	if(!config.PWR_UP) return false;

	WriteReg(FLUSH_RX, NOP);	//清除RX FIFO寄存器
	WriteReg(FLUSH_TX, NOP);	//清除TX FIFO寄存器

	// nRF24L01 在掉电模式下转入发射模式或接收模式前必须经过1.5ms 的待机模式
	Sys.Delay(1500);

	// 如果电源还是关闭，则表示2401初始化失败
	mode = ReadReg(CONFIG);
	config.Init(mode);
	return config.PWR_UP;
}

bool NRF24L01::CheckConfig()
{
	return true;
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
	PortScope ps(_CE);
	WriteReg(CONFIG, config.ToByte());

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

bool NRF24L01::OnOpen()
{
	// 检查并打开Spi
	_spi->Open();

	Error = 0;

	//return Check() && Config() && Check();
	// 配置完成以后，无需再次检查
	if(!(Check() && Config()))
	{
		// 关闭SPI，可能是因为SPI通讯的问题，下次打开模块的时候会重新打开SPI
		//_spi->Close();
		return false;
	}

	// 如果有注册事件，则启用接收任务
	//if(HasHandler()) _taskID = Sys.AddTask(ReceiveTask, this, 0, 1000);
	// 很多时候不需要异步接收数据，如果这里注册了，会导致编译ReceiveTask函数

	return true;
}

void NRF24L01::OnClose()
{
	byte mode = ReadReg(CONFIG);
	RF_CONFIG config;
	config.Init(mode);

	// 关闭电源
	config.PWR_UP = 0;

	PortScope ps(_CE);
	WriteReg(CONFIG, config.ToByte());

	_spi->Close();

	//if(_taskID) Sys.RemoveTask(_taskID);
	//_taskID = 0;
}

// 从NRF的接收缓冲区中读出数据
uint NRF24L01::OnRead(byte *data, uint len)
{
	Lock lock(_Lock);
	//if(!lock.Wait(10000)) return false;
	// 如果拿不到锁，马上退出，不等待。因为一般读取是由定时器中断来驱动，等待没有意义。
	if(!lock.Success) return false;

	// 进入接收模式
	if(!SetMode(true)) return false;

	// 读取status寄存器的值
	Status = ReadReg(STATUS);
	if(Status == 0xFF)
	{
		AddError();
		return 0;
	}
	// 判断是否接收到数据
	RF_STATUS st;
	st.Init(Status);
	if(!st.RX_DR || st.RX_P_NO == 0x07) return 0;
	// 单个2401模块独立工作时，也会收到乱七八糟的数据，通过判断RX_P_NO可以过滤掉一部分
	//if(!st.RX_DR) return 0;
	//debug_printf("NRF24L01::OnRead st=0x%02x\r\n", Status);

	// 清除中断标志
	WriteReg(STATUS, Status);

	ReadBuf(RD_RX_PLOAD, data, PayloadWidth); // 读取数据
	WriteReg(FLUSH_RX, NOP);          // 清除RX FIFO寄存器

	return PayloadWidth;
}

// 向NRF的发送缓冲区中写入数据
bool NRF24L01::OnWrite(const byte* data, uint len)
{
	Lock lock(_Lock);
	if(!lock.Wait(10000)) return false;

	// 进入发送模式
	if(!SetMode(false)) return false;

	// 检查要发送数据的长度
	assert_param(len <= PayloadWidth);
	WriteBuf(WR_TX_PLOAD, data, PayloadWidth);

	bool rs = false;
	// 等待发送完成中断
	//if(!WaitForIRQ()) return false;
	// IRQ不可靠，改为轮询寄存器
	// 这里需要延迟一点时间，发送没有那么快完成
	ulong us = 0;
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
			// 清除TX_DS或MAX_RT中断标志
			WriteReg(STATUS, Status);
			WriteReg(FLUSH_TX, NOP);    // 清除TX FIFO寄存器

			rs = st.TX_DS;

			if(!st.TX_DS && st.MAX_RT) AddError();

			break;
		}

		if(!us) us = Time.Current() + Timeout * 1000;
	}while(us > Time.Current());

	SetMode(true);	// 发送完成以后进入接收模式

	return rs;
}

bool NRF24L01::WaitForIRQ()
{
	ulong us = Time.Current() + Timeout * 1000; // 等待100ms
	while(_IRQ->Read() && us > Time.Current());
	if(us >= Time.Current()) return true;

	// 读取状态寄存器的值
	Status = ReadReg(STATUS);
	if(Status == 0xFF) return false;
	debug_printf("NRF24L01::WaitForIRQ Timeout %dms\r\n", Timeout);

	return false;
}

void NRF24L01::AddError()
{
	Error++;
	if(MaxError > 0 && Error >= MaxError)
	{
		debug_printf("nRF24L01+出错%d次，超过最大次数%d，准备重启模块\r\n", Error, MaxError);

		Close();
		Open();
	}
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
}*/

// 引脚中断函数调用此函数  在NRF24L01::NRF24L01()构造函数中注册的是他  而不是上面一个
// typedef void (*IOReadHandler)(Pin pin, bool down, void* param);
void NRF24L01::OnIRQ(Pin pin, bool down, void* param)
{
	//debug_printf("IRQ down=%d\r\n", down);
	// 必须在down=true才能读取到正确的状态
	if(!down) return;

	NRF24L01* nrf = (NRF24L01*)param;
	if(!nrf) return;

	nrf->ShowStatus();

	// 需要判断锁，如果有别的线程正在读写，则定时器无条件退出。
	if(nrf->Opened && nrf->_Lock == 0)
	{
		byte buf[32];
		uint len = nrf->Read(buf, ArrayLength(buf));
		if(len)
		{
			len = nrf->OnReceive(buf, len);

			// 如果有返回，说明有数据要回复出去
			if(len) nrf->Write(buf, len);
		}
	}
}

void NRF24L01::ShowStatus()
{
	// 读取状态寄存器的值
	Status = ReadReg(STATUS);
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

	RF_FIFO_STATUS fifo;
	*(byte*)&fifo = ReadReg(FIFO_STATUS);

	debug_printf("NRF24L01::FIFO_STATUS=0x%02x ", *(byte*)&fifo);
	if(fifo.RX_EMPTY) debug_printf(" RX FIFO 寄存器空");
	if(fifo.RX_FULL)  debug_printf(" RX FIFO 寄存器满");
	if(fifo.TX_EMPTY) debug_printf(" TX FIFO 寄存器空");
	if(fifo.TX_FULL)  debug_printf(" TX FIFO 寄存器满");
	if(fifo.TX_REUSE) debug_printf(" 当CE位高电平状态时不断发送上一数据包");
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

void NRF24L01::ReceiveTask(void* sender, void* param)
//void NRF24L01::ReceiveTask(void* param)
{
	assert_ptr(param);

	NRF24L01* nrf = (NRF24L01*)param;
	byte buf[32];
	//while(true)
	{
		// 需要判断锁，如果有别的线程正在读写，则定时器无条件退出。
		if(nrf->Opened && nrf->_Lock == 0)
		{
			//nrf->SetMode(true);
			uint len = nrf->Read(buf, ArrayLength(buf));
			if(len)
			{
				len = nrf->OnReceive(buf, len);

				// 如果有返回，说明有数据要回复出去
				if(len) nrf->Write(buf, len);
			}
		}

		//Sys.Sleep(1);
	}
}

void NRF24L01::Register(TransportHandler handler, void* param)
{
	ITransport::Register(handler, param);

	// 如果有注册事件，则启用接收任务
	if(handler)
	{
		//if(!_taskID) _taskID = Sys.AddTask(ReceiveTask, this, 0, 1000);
		// 如果外部没有设定，则内部设定
		//if(!_timer) _timer = new Timer(TIM2);
		/*if(!_timer) _timer = Timer::Create();

		_timer->SetFrequency(1000);
		_timer->Register(ReceiveTask, this);
		_timer->Start();*/

		/*if(!_Thread)
		{
			// 分配1k大小的栈
			_Thread = new Thread(ReceiveTask, this, 0x1000);
			_Thread->Name = "RF2401";
			_Thread->Start();
		}*/
	}
	else
	{
		//if(_taskID) Sys.RemoveTask(_taskID);
		//_taskID = 0;
		/*if(_timer) delete _timer;
		_timer = NULL;*/

		//delete _Thread;
		//_Thread = NULL;
	}
}
