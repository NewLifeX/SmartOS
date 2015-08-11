#ifndef _W5500_H_
#define _W5500_H_

#include "Sys.h"
#include "Stream.h"
#include "Spi.h"
#include "Net\ITransport.h"

// 硬件Socket基类
class HardwareSocket;

// 数据帧格式
// 2byte Address + 1byte CONFIG_Phase + nbyte Data Phase
typedef struct
{
	ushort	Address;
	byte	BSB;		// 5位    CONFIG_Phase 由底下封装  这里只需要知道BSB就好
	Stream	Data;

	void Clear(){ ArrayZero2(this, 3); }
}Frame;

class W5500 //: public ITransport // 只具备IP 以及相关整体配置  不具备Socket发送能力 所以不是ITransport
{
private:
	// 收发数据锁，确保同时只有一个对象使用
	volatile byte _Lock;

	Spi*		_spi;
    InputPort	_IRQ;
	// rst引脚可能不是独享的  这里只留一个指针
	OutputPort* nRest;

	// 8个硬件socket
	HardwareSocket* _sockets[8];

	// 读写帧，帧本身由外部构造   （包括帧数据内部的读写标志）
	void SetAddress(ushort addr, byte reg, byte rw);
	bool WriteFrame(ushort addr, byte reg, const ByteArray& bs);
	bool ReadFrame(ushort addr, byte reg, ByteArray& bs);

	// spi 模式（默认变长）
	ushort		PhaseOM;
	//byte RX_FREE_SIZE;	// 剩余接收缓存 kbyte
	//byte TX_FREE_SIZE;	// 剩余发送缓存 kbyte
public:
	ushort		RetryTime;
	ushort		LowLevelTime;
	byte		RetryCount;
	bool		PingACK;
	bool		EnableDHCP;

	bool		Opened;	// 是否已经打开

	IPAddress	IP;		// 本地IP地址
    IPAddress	Mask;	// 子网掩码
	MacAddress	Mac;	// 本地Mac地址

	IPAddress	DHCPServer;
	IPAddress	DNSServer;
	IPAddress	Gateway;

	// 构造
	W5500();
    W5500(Spi* spi, Pin irq = P0 ,OutputPort* rst = NULL);	// 必须具备复位引脚 否则寄存器不能读
    ~W5500();

	// 初始化
	void Init();
    void Init(Spi* spi, Pin irq = P0, OutputPort* rst = NULL);	// 必须给出 rst 控制引脚

	bool Open();
	bool Close();

	// 复位 包含硬件复位和软件复位
	void Reset();

	// 网卡状态输出
	void StateShow();
	// 测试PHY状态  返回是否连接网线
	bool CheckLink();
	// 输出物理链路层状态
	void PhyStateShow();

	// 设置网关IP
	void SetGateway(IPAddress& ip);
	// 设置默认网关IP
	void DefGateway();
	// 获取网关IP
	//IPAddress GetGateway(){ _ip.Value =  *(uint*)General_reg.GAR; return _ip; };

	// 子网掩码
	void SetIpMask(IPAddress& mask);
	// 设置默认子网掩码
	void DefIpMask();
	// 获取子网掩码
	//IPAddress GetIpMask(){ _ip.Value =  *(uint*)General_reg.SUBR; return _ip; };

	// 设置自己的IP
	void SetMyIp(IPAddress& ip);
	// 获取自己的IP
	//IPAddress GetMyIp(){ _ip.Value =  *(uint*)General_reg.SIPR; return _ip; };

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
	void Register(byte Index, HardwareSocket* handler);
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
