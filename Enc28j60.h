#ifndef __Enc28j60_H__
#define __Enc28j60_H__

#include "Sys.h"
#include "Spi.h"
#include "Net\ITransport.h"

// Enc28j60类
class Enc28j60 : public ITransport
{
private:
    Spi* _spi;
    OutputPort _ce;
	OutputPort _reset;

    uint NextPacketPtr;

public:
	byte Mac[6];
    byte Bank;

	Enc28j60();
    virtual ~Enc28j60();
	void Init(Spi* spi, Pin ce = P0, Pin reset = P0);

    byte ReadOp(byte op, byte addr);
    void WriteOp(byte op, byte addr, byte data);
    void ReadBuffer(byte* buf, uint len);
    void WriteBuffer(const byte* buf, uint len);
	// 设定寄存器地址区域
    void SetBank(byte addr);
	// 读取寄存器值 发送读寄存器命令和地址
    byte ReadReg(byte addr);
	// 写寄存器值 发送写寄存器命令和地址
    void WriteReg(byte addr, byte data);
    uint PhyRead(byte addr);
    bool PhyWrite(byte addr, uint data);
    void ClockOut(byte clock);
	bool Linked();

    void Init(byte mac[6]);
    byte GetRevision();

protected:
	virtual bool OnOpen();
    virtual void OnClose() { }

    virtual bool OnWrite(const byte* buf, uint len);
	virtual uint OnRead(byte* buf, uint len);
};

#endif
