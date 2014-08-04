#ifndef __NRF24L01_H__
#define __NRF24L01_H__

#include "Sys.h"
#include "Spi.h"

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
    virtual ~NRF24L01();

    bool Check();
	void Config(bool isReceive);
    void SetMode(bool isReceive);

    byte Send(byte* data);
    byte Receive(byte* data);
};

#endif
