#ifndef __NRF24L01_H__
#define __NRF24L01_H__

#include "Sys.h"
#include "Spi.h"

#define MAX_TX  		0x10  //达到最大发送次数中断
#define TX_OK   		0x20  //TX发送完成中断
#define RX_OK   		0x40  //接收到数据中断
#define NO_NEWS			0x50  //没有数据在2401中

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

	typedef void (*IRQHandler)(Pin pin, bool down, void* param);

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

	// 数据接收委托，一般param用作目标对象
	typedef void (*DataReceived)(NRF24L01* sp, void* param);
    void Register(DataReceived handler, void* param = NULL);
	void OnReceive(Pin pin, bool down);

private:
	DataReceived _Received;
	void* _Param;

	static void OnReceive(Pin pin, bool down, void* param);
};

#endif
