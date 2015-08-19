#ifndef _W5500_H_
#define _W5500_H_

#include "Sys.h"
#include "Stream.h"
#include "Spi.h"
#include "Net\ITransport.h"
#include "Net\Net.h"

// 硬件Socket基类
class HardSocket;

// W5500以太网驱动
class W5500
{
private:
	// 收发数据锁，确保同时只有一个对象使用
	volatile byte _Lock;

	Spi*		_spi;
    InputPort	_IRQ;
	// rst引脚可能不是独享的  这里只留一个指针
	OutputPort* nRest;

	// 8个硬件socket
	HardSocket* _sockets[8];

	// spi 模式（默认变长）
	ushort		PhaseOM;
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

	bool WriteByte(ushort addr, byte dat, byte socket = 0 ,byte block = 0);
	byte ReadByte(ushort addr, byte socket = 0 ,byte block = 0);
	bool WriteByte2(ushort addr, ushort dat, byte socket = 0 ,byte block = 0);
	ushort ReadByte2(ushort addr, byte socket = 0 ,byte block = 0);

	// 读写帧，帧本身由外部构造   （包括帧数据内部的读写标志）
	void SetAddress(ushort addr, byte rw, byte socket = 0 ,byte block = 0);
	bool WriteFrame(ushort addr, const ByteArray& bs, byte socket = 0 ,byte block = 0);
	bool ReadFrame(ushort addr, ByteArray& bs, byte socket = 0 ,byte block = 0);

	// 复位 包含硬件复位和软件复位
	void Reset();

	// 网卡状态输出
	void StateShow();
	// 测试PHY状态  返回是否连接网线
	bool CheckLink();
	// 输出物理链路层状态
	void PhyStateShow();

private:
	// 中断脚回调
	static void OnIRQ(Pin pin, bool down, void* param);
	void OnIRQ();

public:
	string ToString() { return "W5500"; }

	byte GetSocket();
	void Register(byte Index, HardSocket* handler);
};

// 硬件Socket控制器
class HardSocket : public ITransport
{
private:

public:
	bool Enable;	// 启用
	byte Index;		// 使用的硬Socket编号   也是BSB选项的一部分
	byte Protocol;	// 协议

	W5500*	Host;	// W5500公共部分控制器
	uint _tidRecv;	// 收数据线程

	IPEndPoint	Remote;		// 远程地址。默认发送数据的目标地址
	IPEndPoint	Local;		// 本地地址

	HardSocket(W5500* host, byte protocol);
	virtual ~HardSocket();
	
	// 网卡状态输出
	void StateShow();

	// 打开Socket
	virtual bool OnOpen();
	virtual void OnClose();

	bool WriteByteArray(const ByteArray& bs);
	int ReadByteArray(ByteArray& bs);
	
	virtual bool OnWrite(const byte* buf, uint len);
	virtual uint OnRead(byte* buf, uint len);
	
	static void ReceiveTask(void* param);
	virtual void Register(TransportHandler handler, void* param);
	
	// 恢复配置
	virtual void Recovery();
	// 处理一些不需要异步处理的中断 减少异步次数 
	virtual void IRQ_Process() = 0;
	// 用户注册的中断事件处理 异步调用
	virtual void OnIRQ() = 0;
};

class TcpClient : public HardSocket
{
public:
	TcpClient(W5500* host) : HardSocket(host, 0x01) { }

	virtual bool OnOpen();
	virtual void OnClose();
	bool Listen();
	// 恢复配置，还要维护连接问题
	virtual void Recovery();
	// 中断分发  维护状态
	virtual void IRQ_Process();
	// 用户注册的中断事件处理 异步调用
	virtual void OnIRQ();
};
// UDP接收到的数据结构： RemoteIP(4 byte) + RemotePort(2 byte) + Length(2 byte) + Data(Length byte)
class UdpClient : public HardSocket
{
public:
	UdpClient(W5500* host) : HardSocket(host, 0x02) { }
	// 中断分发  维护状态
	virtual void IRQ_Process();
	// 用户注册的中断事件处理 异步调用
	virtual void OnIRQ();
};

#endif
