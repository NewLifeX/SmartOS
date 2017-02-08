#ifndef _W5500_H_
#define _W5500_H_

#include "Net\NetworkInterface.h"
#include "Net\Socket.h"

// W5500以太网驱动
class W5500 : public NetworkInterface
{
public:
	ushort		RetryTime;
	ushort		LowLevelTime;
	byte		RetryCount;
	bool		PingACK;

	uint		TaskID;

	IDataPort*	Led;	// 指示灯

	// 构造
	W5500();
    W5500(Spi* spi, Pin irq, Pin rst);
	W5500(SPI spi, Pin irq, Pin rst);
    virtual ~W5500();

	// 初始化
	void Init();
    void Init(Spi* spi, Pin irq, Pin rst);
	void SetLed(Pin led);
	void SetLed(OutputPort& led);

	virtual void Config();

	// 读写帧，帧本身由外部构造   （包括帧数据内部的读写标志）
	bool WriteFrame(ushort addr, const Buffer& bs, byte socket = 0 ,byte block = 0);
	bool ReadFrame(ushort addr, Buffer& bs, byte socket = 0 ,byte block = 0);

	// 复位 包含硬件复位和软件复位
	void Reset();

	// 电源等级变更（如进入低功耗模式）时调用
	virtual void ChangePower(int level);

	// 网卡状态输出
	void StateShow();
	// 测试PHY状态  返回是否连接网线
	virtual bool CheckLink();
	// 输出物理链路层状态
	void PhyStateShow();

	cstring ToString() const { return "W5500"; }

	virtual Socket* CreateSocket(NetType type);

	// DNS解析。默认仅支持字符串IP地址解析
	virtual IPAddress QueryDNS(const String& domain);
	// 启用DNS
	virtual bool EnableDNS();
	// 启用DHCP
	virtual bool EnableDHCP();

private:
	friend class HardSocket;
	friend class TcpClient;
	friend class UdpClient;

	Spi*		_spi;
    InputPort	Irq;
	OutputPort	Rst;

	// 8个硬件socket
	List<Socket*>	Sockets;

	// spi 模式（默认变长）
	ushort		PhaseOM;

	typedef IPAddress (*DnsHandler)(NetworkInterface* host, const String& domain);
	DnsHandler	_Dns;	// 解析域名为IP地址
	void*	_Dhcp;

	bool WriteByte(ushort addr, byte dat, byte socket = 0 ,byte block = 0);
	bool WriteByte2(ushort addr, ushort dat, byte socket = 0 ,byte block = 0);
	byte ReadByte(ushort addr, byte socket = 0 ,byte block = 0);
	ushort ReadByte2(ushort addr, byte socket = 0 ,byte block = 0);

	void SetAddress(ushort addr, byte rw, byte socket = 0 ,byte block = 0);

	// 打开与关闭
	virtual bool OnOpen();
	virtual void OnClose();
	// 检测连接
	virtual bool OnLink(uint retry);

	// 中断脚回调
	void OnIRQ(InputPort& port, bool down);
	void OnIRQ();
};

#endif
