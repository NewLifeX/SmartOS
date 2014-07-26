#ifndef __NRF24L01_H__
#define __NRF24L01_H__

#include "Sys.h"
#include "Spi.h"

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
    OutputPort* _CE;
    InputPort* _IRQ;

    byte WriteBuf(byte reg ,byte *pBuf,byte bytes);
    byte ReadBuf(byte reg,byte *pBuf,byte bytes);
    byte ReadReg(byte reg);
    byte WriteReg(byte reg, byte dat);

    void CEUp();
    void CEDown();
public:
    int Channel;    // 通讯频道

    NRF24L01(Spi* spi, Pin ce = P0, Pin irq = P0);
    virtual ~NRF24L01()
    {
        if(_spi) delete _spi;
        if(_CE) delete _CE;
        if(_IRQ) delete _IRQ;
    }

    byte Check(void);
    void EnterSend();
    void EnterReceive();

    byte Send(byte* data);
    byte Receive(byte* data);
};

#endif
