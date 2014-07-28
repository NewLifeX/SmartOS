#include "Sys.h"
#include "Port.h"
#include "NRF24L01.h"

byte RX_BUF[RX_PLOAD_WIDTH];		//接收数据缓存
byte TX_BUF[TX_PLOAD_WIDTH];		//发射数据缓存


/********************************************************************************/
//NRF24L01寄存器操作命令
#define READ_REG_NRF        0x00  //读配置寄存器,低5位为寄存器地址
#define WRITE_REG_NRF       0x20  //写配置寄存器,低5位为寄存器地址
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

#define MAX_TX  		0x10  //达到最大发送次数中断
#define TX_OK   		0x20  //TX发送完成中断
#define RX_OK   		0x40  //接收到数据中断

/*发送  接受   地址*/
byte TX_ADDRESS[TX_ADR_WIDTH] = TX_Address;
byte RX_ADDRESS[RX_ADR_WIDTH] = RX_Address;

/********************************************************************************/

//nRF2401 状态  供委托使用
enum nRF_state
{
	nrf_mode_rx,
	nrf_mode_tx,
	nrf_mode_free
}
nRF24L01_status=nrf_mode_free;

//2401委托函数
void nRF24L01_irq(Pin pin, bool opk);

NRF24L01::NRF24L01(Spi* spi, Pin ce, Pin irq)
{
    //_CE = ce;
	//if(_CE != P0) Port::SetOutput(_CE, false);
    if(ce != P0) _CE = new OutputPort(ce);

    //_IRQ = irq;
    if(irq != P0)
    {
        // 中断引脚初始化
        //Port::Set(_IRQ, Port::Mode_IN, false, Port::Speed_10MHz , Port::PuPd_UP);
        // 中断引脚申请委托
        //Port::Register(_IRQ, nRF24L01_irq);
        _IRQ = new InputPort(irq, false, 10, InputPort::PuPd_UP);
        _IRQ->Register(nRF24L01_irq);
    }

    // 必须先赋值，后面WriteReg需要用到
    _spi = spi;

    WriteReg(FLUSH_RX, 0xff);   // 清除RX FIFO寄存器
	WriteReg(FLUSH_TX, 0xff);   // 清除RX FIFO寄存器
}

/**
  * @brief   用于向NRF的寄存器中写入一串数据
  * @param
  *		@arg reg : NRF的命令+寄存器地址
  *		@arg pBuf：存储了将要写入写寄存器数据的数组，外部定义
  * 	@arg bytes: pBuf的数据长度
  * @retval  NRF的status寄存器的状态
  */
byte NRF24L01::WriteBuf(byte reg ,byte *pBuf,byte bytes)
{
	byte status,byte_cnt;
	CEDown();

	_spi->Start();

    /*发送寄存器号*/
	status = _spi->Write(reg);
  	  /*向缓冲区写入数据*/
	for(byte_cnt=0;byte_cnt<bytes;byte_cnt++)
	//	SPI_NRF_RW(*pBuf++);	//写数据到缓冲区
		_spi->Write(*pBuf++);

	_spi->Stop();
    
    delete _spi;

  	return (status);	//返回NRF24L01的状态
}

/**
  * @brief   用于向NRF的寄存器中写入一串数据
  * @param
  *		@arg reg : NRF的命令+寄存器地址
  *		@arg pBuf：用于存储将被读出的寄存器数据的数组，外部定义
  * 	@arg bytes: pBuf的数据长度
  * @retval  NRF的status寄存器的状态
  */
byte NRF24L01::ReadBuf(byte reg,byte *pBuf,byte bytes)
{
 	byte status, byte_cnt;

	CEDown();

	_spi->Start();

	/*发送寄存器号*/
	status = _spi->Write(reg);
 	/*读取缓冲区数据*/
	for(byte_cnt=0;byte_cnt<bytes;byte_cnt++)
	  pBuf[byte_cnt] = _spi->Write(NOP); //从NRF24L01读取数据

	_spi->Stop();

 	return status;		//返回寄存器状态值
}

/**
  * @brief  主要用于NRF与MCU是否正常连接
  * @param  无
  * @retval SUCCESS/ERROR 连接正常/连接失败
  */
byte NRF24L01::Check(void)
{
	//byte buf[5]={0xC2,0xC2,0xC2,0xC2,0xC2};
	byte buf[5]={0XA5,0XA5,0XA5,0XA5,0XA5};
	byte buf1[5];
	byte i;
	/*写入5个字节的地址.  */
	WriteBuf(WRITE_REG_NRF + TX_ADDR,buf,5);
	/*读出写入的地址 */
	ReadBuf(TX_ADDR,buf1,5);
	/*比较*/
	for(i=0;i<5;i++)
	{
		if(buf1[i]!=0xC2)
		break;
	}
	if(i==5)
		return SUCCESS ;        //MCU与NRF成功连接
	else
		return ERROR ;        //MCU与NRF不正常连接
}

/**
  * @brief   用于从NRF特定的寄存器读出数据
  * @param
  *		@arg reg:NRF的命令+寄存器地址
  * @retval  寄存器中的数据
  */
byte NRF24L01::ReadReg(byte reg)
{
 	byte reg_val;
	CEDown();

	_spi->Start();

  	 /*发送寄存器号*/
	_spi->Write(reg);
	 /*读取寄存器的值 */
	reg_val =  _spi->Write(NOP);

	_spi->Stop();

	return reg_val;
}

/**
  * @brief   用于向NRF特定的寄存器写入数据
  * @param
  *		@arg reg:NRF的命令+寄存器地址
  *		@arg dat:将要向寄存器写入的数据
  * @retval  NRF的status寄存器的状态
  */
byte NRF24L01::WriteReg(byte reg,byte dat)
{
 	byte status;
	CEDown();

	_spi->Start();

	/*发送命令及寄存器号 */
	status = _spi->Write(reg);
	 /*向寄存器写入数据*/
    _spi->Write(dat);

	_spi->Stop();

	/*返回状态寄存器的值*/
   	return status;
}

/**
  * @brief  配置并进入接收模式
  * @param  无
  * @retval 无
  */
void NRF24L01::EnterReceive(void)
{
	CEDown();
	WriteBuf(WRITE_REG_NRF + RX_ADDR_P0,RX_ADDRESS,RX_ADR_WIDTH);//写RX节点地址
	WriteReg(WRITE_REG_NRF + EN_AA,0x01);    //使能通道0的自动应答
	WriteReg(WRITE_REG_NRF + EN_RXADDR,0x01);//使能通道0的接收地址
	WriteReg(WRITE_REG_NRF + RF_CH, Channel);      //设置RF通信频率
	WriteReg(WRITE_REG_NRF + RX_PW_P0,RX_PLOAD_WIDTH);//选择通道0的有效数据宽度
    //WriteReg(WRITE_REG_NRF + RF_SETUP,0x0f); //设置TX发射参数,0db增益,2Mbps,低噪声增益开启
	WriteReg(WRITE_REG_NRF + RF_SETUP,0x07);  //设置TX发射参数,0db增益,1Mbps,低噪声增益开启
	WriteReg(WRITE_REG_NRF + CONFIG, 0x0f);  //配置基本工作模式的参数;  PWR_UP,  EN_CRC, 16BIT_CRC, 接收模式
    /*CE拉高，进入接收模式*/
	CEUp();
	nRF24L01_status = nrf_mode_rx;
}

/**
  * @brief  配置发送模式
  * @param  无
  * @retval 无
  */
void NRF24L01::EnterSend(void)
{
	CEDown();
	WriteBuf(WRITE_REG_NRF + TX_ADDR,TX_ADDRESS,TX_ADR_WIDTH);    //写TX节点地址
	WriteBuf(WRITE_REG_NRF + RX_ADDR_P0,RX_ADDRESS,RX_ADR_WIDTH); //设置TX节点地址,主要为了使能ACK
	WriteReg(WRITE_REG_NRF + EN_AA,0x01);     //使能通道0的自动应答
	WriteReg(WRITE_REG_NRF + EN_RXADDR,0x01); //使能通道0的接收地址
	WriteReg(WRITE_REG_NRF + SETUP_RETR,0x1a);//设置自动重发间隔时间:500us + 86us;最大自动重发次数:10次
	WriteReg(WRITE_REG_NRF + RF_CH, Channel);       //设置RF通道为CHANAL
    //WriteReg(WRITE_REG_NRF + RF_SETUP,0x0f);  //设置TX发射参数,0db增益,2Mbps,低噪声增益开启
	WriteReg(WRITE_REG_NRF + RF_SETUP,0x07);  //设置TX发射参数,0db增益,1Mbps,低噪声增益开启
	WriteReg(WRITE_REG_NRF + CONFIG,0x0e);    //配置基本工作模式的参数;  PWR_UP,  EN_CRC,  16BIT_CRC,  发射模式,   开启所有中断
    /*CE拉高，进入发送模式*/
	CEUp();
	nRF24L01_status=nrf_mode_tx;
    Sys.Sleep(500); //CE要拉高一段时间才进入发送模式
}

/*
此处有等待中断的 无限循环   注意
需要修改
*/

/**
  * @brief   用于从NRF的接收缓冲区中读出数据
  * @param
  *		@arg rxBuf ：用于接收该数据的数组，外部定义
  * @retval
  *		@arg 接收结果
  */
byte NRF24L01::Receive(byte *data)
{
	byte state;
	/*等待接收中断*/
//	while(NRF_Read_IRQ()!=0);
	CEUp();
	CEDown();
	/*读取status寄存器的值  */
	state = ReadReg(STATUS);
	/* 清除中断标志*/
	WriteReg(WRITE_REG_NRF + STATUS, state);
	/*判断是否接收到数据*/
	if(state & RX_OK)                                 //接收到数据
	{
        ReadBuf(RD_RX_PLOAD, data, RX_PLOAD_WIDTH);//读取数据
        WriteReg(FLUSH_RX,NOP);          //清除RX FIFO寄存器
        return RX_OK;
	}
	else
		return ERROR;                    //没收到任何数据
}

/**
  * @brief   用于向NRF的发送缓冲区中写入数据
  * @param
  *		@arg txBuf：存储了将要发送的数据的数组，外部定义
  * @retval  发送结果，成功返回TXDS,失败返回MAXRT或ERROR
  */
byte NRF24L01::Send(byte* data)
{
	byte state;
	 /*ce为低，进入待机模式1*/
	CEDown();
	/*写数据到TX BUF 最大 32个字节*/
	WriteBuf(WR_TX_PLOAD, data, TX_PLOAD_WIDTH);
      /*CE为高，txbuf非空，发送数据包 */
	CEUp();
	  /*等待发送完成中断 */
//	while(NRF_Read_IRQ()!=0);
	/*读取状态寄存器的值 */
	state = ReadReg(STATUS);
	 /*清除TX_DS或MAX_RT中断标志*/
	WriteReg(WRITE_REG_NRF + STATUS,state);
	WriteReg(FLUSH_TX,NOP);    //清除TX FIFO寄存器
	 /*判断中断类型*/
    if(state & MAX_TX)                     //达到最大重发次数
        return MAX_TX;
    else if(state & TX_OK)                  //发送完成
        return TX_OK;
    else
        return ERROR;                 //其他原因发送失败
}

void NRF24L01::CEUp()
{
    //if(_CE != P0) Port::Write(_CE, true);
    if(_CE) *_CE = true;
}

void NRF24L01::CEDown()
{
    //if(_CE != P0) Port::Write(_CE, false);
    if(_CE) *_CE = false;
}

//2401委托函数  未完成
void nRF24L01_irq(Pin pin, bool opk)
{
	byte a=3 ,b=5;
	(void)pin;
	(void)opk;
	switch(nRF24L01_status)
	{
        case nrf_mode_rx: a=b; break;
        case nrf_mode_tx: a+=b; break;
        case nrf_mode_free: a-=b; break;
	}
}
