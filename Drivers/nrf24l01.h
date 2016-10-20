#ifndef __NRF24L01_H__
#define __NRF24L01_H__

#include "Sys.h"
#include "Spi.h"
#include "Net\ITransport.h"
#include "Power.h"
#include "Message\DataStore.h"
#include "APP\FlushPort.h"

// NRF24L01类
class NRF24L01 : public ITransport, public Power
{
public:
    byte Channel;		// 通讯频道。物理频率号，在2400MHZ基础上加
	byte Local[5];		// 本地地址
	byte Remote[5];		// 远程地址

	bool DynPayload;	// 动态负载
	bool AutoAnswer;	// 自动应答，默认启用
	ushort Speed;		// 射频数据率，单位kbps，默认250kbps，可选1000kbps/2000kbps，速度越低传输越远
	byte RadioPower;	// 发射功率。共8档，最高0x07代表7dBm最大功率
	bool Master;		// 是否主节点。

	ushort	Error;		// 错误次数，超过最大错误次数则自动重置

	NRF24L01();
    virtual ~NRF24L01();
    void Init(Spi* spi, Pin ce = P0, Pin irq = P0, Pin power = P0);

    bool Check();
	bool Config();		// 完成基础参数设定，默认初始化为发送模式
	bool GetPower();	// 获取当前电源状态
	bool SetPowerMode(bool on);	// 设置当前电源状态。返回是否成功
	bool GetMode();		// 获取当前模式是否接收模式
    bool SetMode(bool isReceive, const Buffer& addr);	// 切换收发模式，不包含参数设定
	void SetAddress();	// 设置地址

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

	typedef int (*FuncBufInt)(const Buffer&);
	FuncBufInt	FixData;// 修正数据的委托

	byte Status;
	byte FifoStatus;
	void ShowStatus();

	IDataPort*	Led;	// 数据灯
	void SetLed(OutputPort& led);
	void SetLed(Pin led);

	//virtual String ToString() const { return String("R24"); }
	virtual String& ToStr(String& str) const { return str + "R24"; }

private:

	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const Buffer& bs);
	virtual uint OnRead(Buffer& bs);

	// 引发数据到达事件
	//virtual uint OnReceive(Buffer& bs, void* param);
	virtual bool OnWriteEx(const Buffer& bs, const void* opt);
	
	bool SendTo(const Buffer& bs, const Buffer& addr);

    Spi*		_spi;
    OutputPort	_CE;
    InputPort	Irq;
	OutputPort	_Power;	// 设置控制2401电源的引脚  直接进行对2401的通断电操作，以免死机对setPower无效

    byte WriteBuf(byte reg, const Buffer& bs);
    byte ReadBuf(byte reg, Buffer& bs);
    byte ReadReg(byte reg);
    byte WriteReg(byte reg, byte dat);

	void AddError();

	// 接收任务。
	static void ReceiveTask(void* param);
	uint _tidOpen;
	uint _tidRecv;
	void OnIRQ(InputPort& port, bool down);
	void OnIRQ();
};

#endif
