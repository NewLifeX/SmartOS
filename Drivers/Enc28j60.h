#ifndef __Enc28j60_H__
#define __Enc28j60_H__

#include "Sys.h"
#include "Spi.h"
#include "Power.h"
#include "Net\ITransport.h"
#include "Net\Net.h"

// Enc28j60类
class Enc28j60 : public ITransport
{
private:
    Spi* _spi;
    OutputPort _ce;
	OutputPort _reset;

    uint NextPacketPtr;

	ulong	LastTime;		// 记录最后一次收到数据的时间，超时重启
	uint	ResetPeriod;	// 重启间隔，默认6000微秒
	uint	_ResetTask;		// 重启任务
public:
	//byte Mac[6];
	MacAddress Mac;
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
    ushort PhyRead(byte addr);
    bool PhyWrite(byte addr, ushort data);
    void ClockOut(byte clock);
	bool Linked();

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

    //void Init(byte mac[6]);
    byte GetRevision();
	
	bool Broadcast;
	// 设置是否接收广播数据包
	void SetBroadcast(bool flag);
	// 显示寄存器状态
	void ShowStatus();

	int	Error;	// 错误次数
	void CheckError();
protected:
	virtual bool OnOpen();
    virtual void OnClose() { }

    virtual bool OnWrite(const Array& bs);
	virtual uint OnRead(Array& bs);
};

#endif
