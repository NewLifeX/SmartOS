#ifndef _W5500_H_
#define _W5500_H_

#include "Sys.h"
#include "Type.h"
#include "List.h"
#include "Stream.h"
#include "Spi.h"
#include "Net\ITransport.h"
//#include "Thread.h"

#if defined(W500DEBUG)

#define w5500_printf printf

#else

__inline void w5500_printf( const char *format, ... ) {}

#endif


// 硬件Socket基类
class HardwareSocket;

// 数据帧格式
// 2byte Address + 1byte CONFIG_Phase + nbyte Data Phase
typedef struct
{
	ushort	Address;
	byte	BSB;		// 5位    CONFIG_Phase 由底下封装  这里只需要知道BSB就好
	Stream	Data;
	void Clear(){ ArrayZero2(this,3);};
}Frame;

class W5500 //: public ITransport // 只具备IP 以及相关整体配置  不具备Socket发送能力 所以不是ITransport
{	
	// 通用寄存器结构
	struct{
		byte MR;			// 模式			0x0000
		byte GAR[4];		// 网关地址		0x0001
		byte SUBR[4];		// 子网掩码		0x0005
		byte SHAR[6];		// 源MAC地址	0x0009
		byte SIPR[4];		// 源IP地址		0x000f
		byte INTLEVEL[2];	// 低电平中断定时器寄存器	0x0013
		byte IR;			// 中断寄存器				0x0015
		byte IMR;			// 中断屏蔽寄存器			0x0016
		byte SIR;			// Socket中断寄存器			0x0017
		byte SIMR;			// Socket中断屏蔽寄存器		0x0018
		byte RTR[2];		// 重试时间					0x0019
		byte RCR;			// 重试计数					0x001b
		byte PTIMER;		// PPP 连接控制协议请求定时寄存器	0x001c
		byte PMAGIC;		// PPP 连接控制协议幻数寄存器		0x001d
		byte PHAR[6];		// PPPoE 模式下目标 MAC 寄存器		0x001e
		byte PSID[2];		// PPPoE 模式下会话 ID 寄存器		0x0024
		byte PMRU[2];		// PPPoE 模式下最大接收单元			0x0026
		byte UIPR[4];		// 无法抵达 IP 地址寄存器【只读】	0x0028
		byte UPORTR[2];		// 无法抵达端口寄存器【只读】		0x002c
		byte PHYCFGR;		// PHY 配置寄存器					0x002e
		//byte VERSIONR		// 芯片版本寄存器【只读】			0x0039	// 地址不连续
	}General_reg;			// 只有一份 所以直接定义就好
	
private:
	Spi* _spi;
    InputPort	_IRQ;
	// 8个硬件socket
	HardwareSocket* _socket[8];	
	// 收发数据锁，确保同时只有一个对象使用
	int _Lock;			
	// 读写帧，帧本身由外部构造   （包括帧数据内部的读写标志）
	bool WriteFrame(Frame& fra);
	bool ReadFrame(Frame& fra,uint length);
	// spi 模式（默认变长）
	byte PhaseOM;		
public:
	// 非必要，由外部定义  这里只留一个指针
	OutputPort* nRest;
	void SoftwareReset();
	void Reset();
	
	// 构造
	W5500();
    W5500(Spi* spi, Pin irq = P0);
    ~W5500();
	
	// 初始化
	void Init();
    void Init(Spi* spi, Pin irq = P0);
	// 输出 General_reg
	void StateShow();
	
	bool SetMac(MacAddress& mac);		// 本地MAC
	//MacAddress GetMac();
	//void SetIpMask();		// 子网掩码
	//void SetGateway();	// 网关地址
	//void OpenPingACK();	// 开启PING应答
	//void OpenWol();		// 网络唤醒
private:
	// 中断脚回调
	static void OnIRQ(Pin pin, bool down, void* param);		
	void OnIRQ();

public:
	string ToString() { return "W5500"; }
	
	byte GetSocket();
	void Register(byte Index,HardwareSocket* handler);
};

// 硬件Socket基类s
class HardwareSocket
{
private:
	W5500*	_THard;	// 硬件控制器
public:
	//IP_TYPE	Type;	// 类型
	bool Enable;	// 启用
	byte Index;		// 使用的硬Socket编号

	HardwareSocket(W5500* thard);
	virtual ~HardwareSocket();

	// 处理数据包
	virtual bool Process(Stream& ms) = 0;
};

#endif
