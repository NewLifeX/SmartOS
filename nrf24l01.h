#ifndef __NRF24L01_H__
#define __NRF24L01_H__

#include "Sys.h"
#include "Spi.h"
#include "Net\ITransport.h"
#include "Timer.h"
#include "Thread.h"

// NRF24L01类
class NRF24L01 : public ITransport
{
private:
    Spi*		_spi;
    OutputPort*	_CE;
    InputPort*	_IRQ;

    byte WriteBuf(byte reg, const byte *pBuf, byte bytes);
    byte ReadBuf(byte reg, byte *pBuf, byte bytes);
    byte ReadReg(byte reg);
    byte WriteReg(byte reg, byte dat);

	bool WaitForIRQ();
	void AddError();

	// 接收任务。
	static void ReceiveTask(void* sender, void* param);
	//static void ReceiveTask(void* param);
	//uint _taskID;
	//Timer* _timer;		// 使用硬件定时器，取得比主线程更高的优先级
	//Thread* _Thread;
	static void OnIRQ(Pin pin, bool down, void* param);

	int _Lock;			// 收发数据锁，确保同时只有一个对象使用

public:
    byte Channel;		// 通讯频道。物理频率号，在2400MHZ基础上加0x28 MHZ
	byte Address[5];	// 通道0地址
	byte Address1[5];	// 通道1地址
	byte Address2_5[4];	// 通道2_5地址低字节，高4字节跟通道1一致

	uint Timeout;		// 超时时间ms
	byte PayloadWidth;	// 负载数据宽度，默认32字节
	bool AutoAnswer;	// 自动应答，默认启用
	byte Retry;			// 重试次数，最大15次
	ushort RetryPeriod;	// 重试间隔，250us的倍数，最小250us
	ushort MaxError;	// 最大错误次数，超过该次数则自动重置，0表示不重置，默认10
	ushort Error;		// 错误次数，超过最大错误次数则自动重置

    NRF24L01(Spi* spi, Pin ce = P0, Pin irq = P0);
    virtual ~NRF24L01();

    bool Check();
	bool Config();		// 完成基础参数设定，默认初始化为发送模式
    bool SetMode(bool isReceive);	// 切换收发模式，不包含参数设定
	bool CheckConfig();

	byte Status;
	void ShowStatus();
	bool CanReceive();

	virtual void Register(TransportHandler handler, void* param = NULL);

	virtual string ToString() { return "nRF24L01+"; }

protected:
	virtual bool OnOpen();
    virtual void OnClose();

    virtual bool OnWrite(const byte* buf, uint len);
	virtual uint OnRead(byte* buf, uint len);

public:
	class ByteStruct
	{
	public:
		void Init(byte data = 0) { *(byte*)this = data; }
		byte ToByte() { return *(byte*)this; }
	};

	// 配置寄存器0x00
	typedef struct : ByteStruct
	{
		byte PRIM_RX:1;	// 1:接收模式 0:发射模式
		byte PWR_UP:1;	// 1:上电 0:掉电
		byte CRCO:1;	// CRC 模式‘0’-8 位CRC 校验‘1’-16 位CRC 校验
		byte EN_CRC:1;	// CRC 使能如果EN_AA 中任意一位为高则EN_CRC 强迫为高
		byte MAX_RT:1;	// 可屏蔽中断MAX_RT, 1 IRQ 引脚不显示TX_DS 中断, 0 MAX_RT 中断产生时IRQ 引脚电平为低
		byte TX_DS:1;	// 可屏蔽中断TX_DS, 1 IRQ 引脚不显示TX_DS 中断, 0 TX_DS 中断产生时IRQ 引脚电平为低
		byte RX_DR:1;	// 可屏蔽中断RX_RD, 1 IRQ 引脚不显示RX_RD 中断, 0 RX_RD 中断产生时IRQ 引脚电平为低
		byte Reserved:1;
	}RF_CONFIG;

	// 自动应答0x01
	typedef struct : ByteStruct
	{
		byte ENAA_P0:1;	// 数据通道0 自动应答允许
		byte ENAA_P1:1;	// 数据通道1 自动应答允许
		byte ENAA_P2:1;	// 数据通道2 自动应答允许
		byte ENAA_P3:1;	// 数据通道3 自动应答允许
		byte ENAA_P4:1;	// 数据通道4 自动应答允许
		byte ENAA_P5:1;	// 数据通道5 自动应答允许
		byte Reserved:2;
	}RF_EN_AA;

	// 接收地址允许0x02
	typedef struct : ByteStruct
	{
		byte ERX_P0:1;	// 接收数据通道0 允许
		byte ERX_P1:1;	// 接收数据通道1 允许
		byte ERX_P2:1;	// 接收数据通道2 允许
		byte ERX_P3:1;	// 接收数据通道3 允许
		byte ERX_P4:1;	// 接收数据通道4 允许
		byte ERX_P5:1;	// 接收数据通道5 允许
		byte Reserved:2;
	}RF_EN_RXADDR;

	// 设置地址宽度(所有数据通道)0x03
	typedef struct : ByteStruct
	{
		byte AW:2;	// 接收/发射地址宽度‘00’-无效‘01’-3 字节宽度‘10’-4 字节宽度‘11’-5 字节宽度
		byte Reserved:6;
	}RF_SETUP_AW;

	// 建立自动重发0x04
	typedef struct : ByteStruct
	{
		byte ARC:4;	// 自动重发计数‘0000’-禁止自动重发‘0000’-自动重发一次……‘0000’-自动重发15 次
		byte ARD:4;	// 自动重发延时‘0000’-等待250+86us‘0001’-等待500+86us‘0010’-等待750+86us……‘1111’-等待4000+86us
					// (延时时间是指一包数据发送完成到下一包数据开始发射之间的时间间隔)
	}RF_SETUP_RETR;

	// 射频通道0x05
	typedef struct : ByteStruct
	{
		byte CH:7;	// 设置nRF24L01 工作通道频率
		byte Reserved:1;
	}RF_CH;

	// 射频寄存器0x06
	typedef struct : ByteStruct
	{
		//byte LNA_HCURR:1;	// 低噪声放大器增益
		//byte POWER:2;		// 发射功率‘00’ -18dBm‘01’ -12dBm‘10’ -6dBm‘11’ 0dBm
		byte POWER:3;		// 台产版发射功率‘000’ -12dBm/-6/-4/0/1/3/4  '111' 7dBm
		byte DR:1;			// 数据传输率‘0’ –1Mbps ‘1’ 2 Mbps。台产版跟DR_HIGH配合
		byte PLL_LOCK:1;	// PLL_LOCK 允许仅应用于测试模式。台产必须为0
		byte DR_HIGH:1;		// 射频数据率 [DR, DR_HIGH]: 00：1Mbps 01：2Mbps 10：250kbps 11：保留
		byte Reserved:2;
	}RF_SETUP;

	// 状态寄存器0x07
	typedef struct : ByteStruct
	{
		byte TX_FULL:1;	// TX FIFO 寄存器满标志, 1:TX FIFO 寄存器满, 0: TX FIFO 寄存器未满,有可用空间
		byte RX_P_NO:3;	// 接收数据通道号, 000-101:数据通道号, 110:未使用, 111:RX FIFO 寄存器为空
		byte MAX_RT:1;	// 达到最多次重发中断, 写‘1’清除中断, 如果MAX_RT 中断产生则必须清除后系统才能进行通讯
		byte TX_DS:1;	// 数据发送完成中断，当数据发送完成后产生中断。如果工作在自动应答模式下，只有当接收到应答信号后此位置一写‘1’清除中断
		byte RX_DR:1;	// 接收数据中断，当接收到有效数据后置一, 写‘1’清除中断
		byte Reserved:1;
	}RF_STATUS;

	// 发送检测寄存器0x08
	typedef struct : ByteStruct
	{
		byte ARC_CNT:4;	// 重发计数器发送新数据包时此寄存器复位
		byte PLOS_CNT:4;// 数据包丢失计数器当写RF_CH 寄存器时此寄存器复位当丢失15个数据包后此寄存器重启
	}RF_OBSERVE_TX;

	// 载波检测0x09
	typedef struct : ByteStruct
	{
		byte CD:1;	// 载波检测
		byte Reserved:7;
	}RF_CD;

	// 射频通道0x17
	typedef struct : ByteStruct
	{
		byte RX_EMPTY:1;	// RX FIFO 寄存器空标志 1:RX FIFO 寄存器空 0: RX FIFO 寄存器非空
		byte RX_FULL:1;		// RX FIFO 寄存器满标志 1:RX FIFO 寄存器满 0: RX FIFO 寄存器未满有可用空间
		byte Reserved:2;
		byte TX_EMPTY:1;	// TX FIFO 寄存器空标志 1:TX FIFO 寄存器空 0: TX FIFO 寄存器非空
		byte TX_FULL:1;		// TX FIFO 寄存器满标志 1:TX FIFO 寄存器满 0: TX FIFO 寄存器未满有可用空间
		byte TX_REUSE:1;	// 若TX_REUSE=1 则当CE位高电平状态时不断发送上一数据包TX_REUSE 通过SPI 指令REUSE_TX_PL 设置通过W_TX_PALOAD或FLUSH_TX 复位
		byte Reserved2:1;
	}RF_FIFO_STATUS;
};

#endif
