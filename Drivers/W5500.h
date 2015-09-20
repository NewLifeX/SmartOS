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
class W5500 : public ISocketHost
{
private:
	friend class HardSocket;
	friend class TcpClient;
	friend class UdpClient;

	// 收发数据锁，确保同时只有一个对象使用
	volatile byte _Lock;

	Spi*		_spi;
    InputPort	Irq;
	OutputPort	Rst;

	// 8个硬件socket
	HardSocket* _sockets[8];

	// spi 模式（默认变长）
	ushort		PhaseOM;

	bool WriteByte(ushort addr, byte dat, byte socket = 0 ,byte block = 0);
	bool WriteByte2(ushort addr, ushort dat, byte socket = 0 ,byte block = 0);
	byte ReadByte(ushort addr, byte socket = 0 ,byte block = 0);
	ushort ReadByte2(ushort addr, byte socket = 0 ,byte block = 0);

	void SetAddress(ushort addr, byte rw, byte socket = 0 ,byte block = 0);
	void OnClose();

public:
	ushort		RetryTime;
	ushort		LowLevelTime;
	byte		RetryCount;
	bool		PingACK;
	bool		EnableDHCP;

	bool		Opened;	// 是否已经打开

	// 构造
	W5500();
    W5500(Spi* spi, Pin irq = P0 ,Pin rst = P0);	// 必须具备复位引脚 否则寄存器不能读
    ~W5500();

	// 初始化
	void Init();
    void Init(Spi* spi, Pin irq = P0, Pin rst = P0);	// 必须给出 rst 控制引脚

	bool Open();
	bool Close();
	void Config();
	void ShowInfo();

	// 读写帧，帧本身由外部构造   （包括帧数据内部的读写标志）
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
class HardSocket : public ITransport, public ISocket
{
private:
	W5500*	_Host;	// W5500公共部分控制器

protected:
	byte ReadConfig();
	void WriteConfig(byte dat);
	byte ReadStatus();
	byte ReadInterrupt();
	void WriteInterrupt(byte dat);

public:
	bool Enable;	// 启用
	byte Index;		// 使用的硬Socket编号   也是BSB选项的一部分
	byte Protocol;	// 协议

	uint _tidRecv;	// 收数据线程

	HardSocket(W5500* host, byte protocol);
	virtual ~HardSocket();

	// 网卡状态输出
	void StateShow();

	// 打开Socket
	virtual bool OnOpen();
	virtual void OnClose();

	// 应用配置，修改远程地址和端口
	void Change(const IPEndPoint& remote);

	virtual bool OnWrite(const ByteArray& bs);
	virtual uint OnRead(ByteArray& bs);

	// 发送数据
	virtual bool Send(const ByteArray& bs);
	// 接收数据
	virtual uint Receive(ByteArray& bs);

	static void ReceiveTask(void* param);
	virtual void Register(TransportHandler handler, void* param);

	// 恢复配置
	virtual void Recovery();
	// 处理一些不需要异步处理的中断 减少异步次数
	void Process();
	virtual void OnProcess(byte reg) = 0;
	// 用户注册的中断事件处理 异步调用
	virtual void RaiseReceive() = 0;
	// 清空所有接收缓冲区
	void ClearRX();
};

class TcpClient : public HardSocket
{
public:
	TcpClient(W5500* host): HardSocket(host, 0x01){ Init(); };
	void Init();
	virtual ~TcpClient();
	virtual bool OnOpen();
	virtual void OnClose();

	bool Listen();

	// 恢复配置，还要维护连接问题
	virtual void Recovery();
	// 中断分发  维护状态
	virtual void OnProcess(byte reg);
	// 用户注册的中断事件处理 异步调用
	virtual void RaiseReceive();

private:
	bool Linked;
	uint _tidRodyguard;	// 维护 Link 状态的任务
	static void RodyguardTask(void* param);
};

// UDP接收到的数据结构： RemoteIP(4 byte) + RemotePort(2 byte) + Length(2 byte) + Data(Length byte)
class UdpClient : public HardSocket
{
public:
	UdpClient(W5500* host) : HardSocket(host, 0x02) { }

	virtual bool SendTo(const ByteArray& bs, const IPEndPoint& remote);

	// 中断分发  维护状态
	virtual void OnProcess(byte reg);
	// 用户注册的中断事件处理 异步调用
	virtual void RaiseReceive();

private:
};

#endif
