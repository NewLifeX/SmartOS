#ifndef __NRF24L01_H__
#define __NRF24L01_H__

#include "Sys.h"
#include "Spi.h"
/*
#define MAX_TX  		0x10  //达到最大发送次数中断
#define TX_OK   		0x20  //TX发送完成中断
#define RX_OK   		0x40  //接收到数据中断
#define NO_NEWS			0x50  //没有数据在2401中
*/
/*
nRF24L01+  内部有缓存   没有必要收到数据就直接读出来
用个事件标志位就ok了  一个类里面一个
面对多个nRF24L01 的问题  申请中断的时候传入nRF的事例就好
*/


// NRF24L01类
class NRF24L01
{
private:
    Spi* _spi;
    OutputPort* _CE;
    InputPort* _IRQ;
	//bool _isEvent;

    byte WriteBuf(byte reg ,byte *pBuf,byte bytes);
    byte ReadBuf(byte reg,byte *pBuf,byte bytes);
    byte ReadReg(byte reg);
    byte WriteReg(byte reg, byte dat);

	bool WaitForIRQ();

	typedef void (*IRQHandler)(Pin pin, bool down, void* param);

    void CEUp();
    void CEDown();
public:
    int Channel;    // 通讯频道
	byte Address[5];
	uint Timeout;	// 超时时间ms

    NRF24L01(Spi* spi, Pin ce = P0, Pin irq = P0);
    virtual ~NRF24L01();

    bool Check();
	void Config(bool isReceive);
    void SetMode(bool isReceive);

	typedef enum
	{
		Max_TX  = 0x10, // 达到最大发送次数中断
		TX_OK   = 0x20, // TX发送完成中断
		RX_OK   = 0x40, // 接收到数据中断
		//NO_News = 0x50	// 没有数据
	}RF_STATUS;
	RF_STATUS Status;
	
    bool Send(byte* data);
    bool Receive(byte* data);

	// 数据接收委托，一般param用作目标对象
	typedef void (*DataReceived)(NRF24L01* sp, void* param);
    void Register(DataReceived handler, void* param = NULL);
	void OnReceive(Pin pin, bool down);

private:
	DataReceived _Received;
	void* _Param;

	static  void OnReceive(Pin pin, bool down, void* param);
};

#endif
