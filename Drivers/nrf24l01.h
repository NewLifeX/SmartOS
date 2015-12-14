#ifndef __NRF24L01_H__
#define __NRF24L01_H__

#include "Sys.h"
#include "Spi.h"
#include "Net\ITransport.h"
#include "Power.h"
#include "Message\DataStore.h"

// NRF24L01类
class NRF24L01 : public ITransport, public Power
{
private:
    byte WriteBuf(byte reg, const byte *pBuf, byte bytes);
    byte ReadBuf(byte reg, byte *pBuf, byte bytes);
    byte ReadReg(byte reg);
    byte WriteReg(byte reg, byte dat);

	void AddError();

	// 接收任务。
	static void ReceiveTask(void* param);
	uint _tidOpen;
	uint _tidRecv;
	static void OnIRQ(InputPort* port, bool down, void* param);
	void OnIRQ();

	int _Lock;			// 收发数据锁，确保同时只有一个对象使用

public:
    Spi*		_spi;
    OutputPort	_CE;
    InputPort	_IRQ;
	OutputPort	_Power;	// 设置控制2401电源的引脚  直接进行对2401的通断电操作，以免死机对setPower无效

    byte Channel;		// 通讯频道。物理频率号，在2400MHZ基础上加
	byte Address[5];	// 通道0地址
	byte Address1[5];	// 通道1地址
	byte Addr2_5[4];	// 通道2_5地址低字节，高4字节跟通道1一致
	byte AddrBits;		// 使能通道标识位。默认0x01使能地址0

	byte PayloadWidth;	// 负载数据宽度，默认32字节。0表示使用动态负载
	bool AutoAnswer;	// 自动应答，默认启用
	byte Retry;			// 重试次数，最大15次
	ushort RetryPeriod;	// 重试间隔，250us的倍数，最小250us
	ushort Speed;		// 射频数据率，单位kbps，默认250kbps，可选1000kbps/2000kbps，速度越低传输越远
	byte RadioPower;	// 发射功率。共8档，最高0x07代表7dBm最大功率

	uint Timeout;		// 超时时间ms
	ushort MaxError;	// 最大错误次数，超过该次数则自动重置，0表示不重置，默认10
	ushort Error;		// 错误次数，超过最大错误次数则自动重置
	byte	AddrLength;	// 地址长度。默认0，主站建议设为5

	NRF24L01();
    virtual ~NRF24L01();
    void Init(Spi* spi, Pin ce = P0, Pin irq = P0, Pin power = P0);

    bool Check();
	bool Config();		// 完成基础参数设定，默认初始化为发送模式
	bool GetPower();	// 获取当前电源状态
	bool SetPowerMode(bool on);	// 设置当前电源状态。返回是否成功
	bool GetMode();		// 获取当前模式是否接收模式
    bool SetMode(bool isReceive);	// 切换收发模式，不包含参数设定
	void SetAddress(bool full);	// 设置地址。参数指定是否设置0通道地址以外的完整地址

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

	Action		FixData;// 修正数据的委托
	IDataPort*	Led;	// 数据灯

	byte Status;
	byte FifoStatus;
	void ShowStatus();

	virtual string ToString() { return "R24"; }

protected:
	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const Array& bs);
	virtual uint OnRead(Array& bs);

	// 引发数据到达事件
	virtual uint OnReceive(Array& bs, void* param);
	virtual bool OnWriteEx(const Array& bs, void* opt);
};

#endif
