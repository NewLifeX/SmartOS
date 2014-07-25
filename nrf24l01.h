#ifndef __NRF24L01_H__
#define __NRF24L01_H__

#include "Sys.h"
#include "Spi.h"


//使用哪个spi作为 nrf 通信口
//#define 	nRF2401_SPI					SPI_3
//中断引脚
#define 	nRF2401_IRQ_pin		 PD14
//中断引脚检测
#define NRF_Read_IRQ()		  Port::Read(nRF2401_IRQ_pin)
//是否使用CE引脚
#define		us_nrf_ce						1
//CE引脚操作
#if us_nrf_ce
	//CE引脚定义
	//#define   nRF2401_CE					PD13
	//#define 	NRF_CE_LOW()				Port::Write(nRF2401_CE, false)
	//#define 	NRF_CE_HIGH()				Port::Write(nRF2401_CE, true)
#else
	#define 	NRF_CE_LOW()
	#define 	NRF_CE_HIGH()
#endif

//定义缓冲区大小  单位  byte
#define RX_PLOAD_WIDTH				5
#define TX_PLOAD_WIDTH				5

extern unsigned char RX_BUF[];		//接收数据缓存
extern unsigned char TX_BUF[];		//发射数据缓存

/*发送包大小*/
#define TX_ADR_WIDTH	5
#define RX_ADR_WIDTH	5

/*发送 接收数据地址*/
#define TX_Address	{0x34,0x43,0x10,0x10,0x01}
#define RX_Address	{0x34,0x43,0x10,0x10,0x01}

// NRF24L01类
class NRF24L01
{
private:
    Spi* _spi;
    Pin _CE;

    byte WriteBuf(byte reg ,byte *pBuf,byte bytes);
    byte ReadBuf(byte reg,byte *pBuf,byte bytes);
    byte ReadReg(byte reg);
    byte WriteReg(byte reg, byte dat);

    void CEUp();
    void CEDown();
public:
    int Channel;    // 通讯频道

    NRF24L01(Spi* spi, Pin ce = P0);
    ~NRF24L01();

    byte Check(void);
    void EnterSend();
    void EnterReceive();

    byte Send(byte* data);
    byte Receive(byte* data);
};

#endif
