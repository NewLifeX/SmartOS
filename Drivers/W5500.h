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
	struct T_GenReg{
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
	// 收发数据锁，确保同时只有一个对象使用
	volatile byte _Lock;
	// 本地 ip 是否是Dhcp得到的 1 是  0 不是
	//byte IsDhcpIp;
	
	Spi* _spi;
    InputPort	_IRQ;
	// 8个硬件socket
	HardwareSocket* _socket[8];	
	// mac对象
	MacAddress _mac;
	IPAddress _ip;
	// 读写帧，帧本身由外部构造   （包括帧数据内部的读写标志）
	bool WriteFrame(Frame& fra);
	bool ReadFrame(Frame& fra,uint length);
	// spi 模式（默认变长）
	byte PhaseOM;
	byte RX_FREE_SIZE;	// 剩余接收缓存 kbyte
	byte TX_FREE_SIZE;	// 剩余发送缓存 kbyte
public:
	// rst引脚可能不是独享的  这里只留一个指针
	OutputPort* nRest;
	// DHCP服务器IP
	//IPAddress DHCPServer;
	//IPAddress DNSServer;
	
	// 软件复位
	void SoftwareReset();
	// 复位 包含硬件复位和软件复位
	void Reset();
	
	// 构造
	W5500();
    W5500(Spi* spi, Pin irq = P0 ,OutputPort* rst = NULL);	// 必须具备复位引脚 否则寄存器不能读
    ~W5500();
	
	// 初始化
	void Init();
    void Init(Spi* spi, Pin irq = P0, OutputPort* rst = NULL);	// 必须给出 rst 控制引脚
	// 网卡状态输出
	void StateShow();
	
	// 测试PHY状态  返回是否连接网线
	bool CheckLnk();
	// 输出物理链路层状态
	void PhyStateShow();
	
	// 设置本地MAC
	bool SetMac(MacAddress& mac);
	// “随机”一个MAC  并设置
	void AutoMac();
	// 返回 MacAddress
	MacAddress Mac();
	
	// 设置网关IP
	void SetGateway(IPAddress& ip);
	// 设置默认网关IP
	void DefGateway();
	// 获取网关IP
	IPAddress GetGateway(){ _ip.Value =  *(uint*)General_reg.GAR; return _ip; };
	
	// 子网掩码
	void SetIpMask(IPAddress& mask);
	// 设置默认子网掩码
	void DefIpMask();
	// 获取子网掩码
	IPAddress GetIpMask(){ _ip.Value =  *(uint*)General_reg.SUBR; return _ip; };
	
	// 设置自己的IP
	void SetMyIp(IPAddress& ip);
	// 获取自己的IP
	IPAddress GetMyIp(){ _ip.Value =  *(uint*)General_reg.SIPR; return _ip; };
	
	/* 超时时间 = 重试时间*重试次数  */
	// 设置重试时间		超时重传/触发超时中断	最大 6553ms		（默认200ms）
	void SetRetryTime(ushort ms);
	// 设置重试次数		超时重传的次数			最大256			（默认8次）
	void SetRetryCount(byte count);
	
	// 中断时低电平持续时间
	void SetIrqLowLevelTime(int us);
	
	// 开启PING应答
	void OpenPingACK();
	void ClosePingACK();
	
	//void OpenWol();		// 网络唤醒
	void Recovery();
private:
	// 中断脚回调
	static void OnIRQ(Pin pin, bool down, void* param);		
	void OnIRQ();

public:
	string ToString() { return "W5500"; }
	
	byte GetSocket();
	void Register(byte Index,HardwareSocket* handler);
};

// 硬件Socket控制器
class HardwareSocket
{
public:
	enum Protocol
	{
		CLOSE	= 0x00,
		TCP		= 0x01,
		UDP		= 0x02,
		MACRAW	= 0x04,
	};
private:	
	struct T_HSocketReg{
		byte Sn_MR ;		//0x0000  	// Socket 模式寄存器
		byte Sn_CR ;		//0x0001  	// 配置寄存器 	【较为特殊】【只写，读为0x00】
		byte Sn_IR ;		//0x0002  	// 中断寄存器	 写1清0？？
		byte Sn_SR ;		//0x0003  	// 状态寄存器	【只读】
		byte Sn_PORT[2] ;	//0x0004  	// TCP UDP 模式下端口号  OPEN之前配置好
		byte Sn_DHAR[6] ;	//0x0006  	// 目的MAC,SEND_MAC使用;CONNECT/SEND 命令时ARP获取到的MAC
		byte Sn_DIPR[4] ;	//0x000c  	// 目标IP地址
		byte Sn_DPORT[2] ;	//0x0010  	// 目标端口
		byte Sn_MSSR[2] ;	//0x0012  	// TCP UDP 模式下 MTU 最大传输单元大小  默认最大值
										// TCP:1460; UDP:1472; MACRAW:1514;
										// MACRAW 模式时 由于MTU 不在内部处理，默认MTU将会生效
										// PPPoE 模式下 略
										// TCP UDP 模式下，传输数据比 MTU大时，数据将会自动划分成默认MTU 单元大小
		byte Reserved ;		//0x0014  	
		byte Sn_TOS ;		//0x0015  	// IP包头 服务类型 	OPEN之前配置
		byte Sn_TTL ;		//0x0016  	// 生存时间 TTL 	OPEN之前配置
		byte Reserved2[7] ;	//0x0017  	-  0x001d
		byte Sn_RXBUF_SIZE ;//0x001e  	// 接收缓存大小   1 2 4 8 16  单位KByte
		byte Sn_TXBUF_SIZE ;//0x001f  	// 发送缓存大小   1 2 4 8 16  单位KByte
		byte Sn_TX_FSR[2] ;	//0x0020  	// 空闲发送寄存器大小
		byte Sn_TX_RD[2] ;	//0x0022  	// 发送读缓存指针
		byte Sn_TX_WR[2] ;	//0x0024  	// 发送写缓存指针
		byte Sn_RX_RSR[2] ;	//0x0026  	// 空闲接收寄存器大小
		byte Sn_RX_RD[2] ;	//0x0028  	// 发送读缓存指针
		byte Sn_RX_WR[2] ;	//0x002a  	// 发送写缓存指针
		byte Sn_IMR ;		//0x002c  	// 中断屏蔽寄存器  结构跟Sn_IR一样 0屏蔽  1不屏蔽
		byte Sn_FRAG[2] ;	//0x002d  	// IP包头 分段部分  分段寄存器
		
		byte Sn_KPALVTR ;	//0x002f  	// 只在TCP模式下使用  在线时间寄存器  单位：5s
										// 为0 时  手动SEND_KEEP
										// > 0 时  忽略SEND_KEEP操作
	}HSocketReg;
private:
	W5500*	_THard;	// W5500公共部分控制器
public:
	bool Enable;	// 启用
	byte Index;		// 使用的硬Socket编号   也是BSB选项的一部分
	
	HardwareSocket(W5500* thard);
	virtual ~HardwareSocket();
	// 打开Socket
	virtual bool OpenSocket() = 0;
	// 恢复配置
	virtual void Recovery() = 0;
	// 处理数据包
	virtual bool Process() = 0;
};

#endif
