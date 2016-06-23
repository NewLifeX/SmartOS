#ifndef __Esp8266_H__
#define __Esp8266_H__

#include "Port.h"
#include "Net\ITransport.h"
#include "Net\Socket.h"
#include "Message\DataStore.h"
#include "Message\BinaryPair.h"

// 最好打开 Soket 前 不注册中断，以免AT指令乱入到中断里面去  然后信息不对称
// 安信可 ESP8266  模块固件版本 v1.3.0.2
class Esp8266 : public PackPort, public ISocketHost
{
public:
	bool	AutoConn;	// 是否自动连接WiFi，默认false

	IDataPort*	Led;	// 指示灯

    Esp8266(ITransport* port, Pin power = P0, Pin rst = P0);
	virtual ~Esp8266();

	void OpenAsync();
	virtual void Config();

	//virtual const String ToString() const { return String("Esp8266"); }
	virtual ISocket* CreateSocket(ProtocolType type);
	// 启用DNS
	virtual bool EnableDNS();
	// 启用DHCP
	virtual bool EnableDHCP();

/******************************** 基础AT指令 ********************************/
	bool Test();
	bool Reset(bool soft);
	String GetVersion();
	bool Sleep(uint ms);
	bool Echo(bool open);
	// 恢复出厂设置，将擦写所有保存到Flash的参数，恢复为默认参数。会导致模块重启
	bool Restore();

/******************************** WiFi功能指令 ********************************/
	// 获取模式
	SocketMode GetMode();
	// 设置模式。需要重启
	bool SetMode(SocketMode mode);

	// 连接AP相关
	String GetJoinAP();
	bool JoinAP(const String& ssid, const String& pass);
	bool UnJoinAP();
	bool SetAutoConn(bool enable);

	String LoadAPs();
	String GetAP();
	bool SetAP(const String& ssid, const String& pass, byte channel, byte ecn, byte maxConnect, bool hidden);
	// 查询连接到AP的Stations信息。无法查询DHCP接入
	String LoadStations();

	bool GetDHCP(bool* sta, bool* ap);
	bool SetDHCP(SocketMode mode, bool enable);

	MacAddress GetMAC(bool sta);
	bool SetMAC(bool sta, const MacAddress& mac);

	IPAddress GetIP(bool sta);

/******************************** TCP/IP ********************************/
	String GetStatus();
	bool GetMux();
	bool SetMux(bool enable);

	bool Update();

	bool Ping(const IPAddress& ip);

	bool SetIPD(bool enable);

/******************************** 发送指令 ********************************/
	// 发送指令，在超时时间内等待返回期望字符串，然后返回内容
	String Send(const String& cmd, cstring expect, cstring expect2 = nullptr, uint msTimeout = 1000);
	// 发送命令，自动检测并加上\r\n，等待响应OK
	bool SendCmd(const String& cmd, uint msTimeout = 1000);
	bool WaitForCmd(cstring expect, uint msTimeout);

/******************************** 发送指令 ********************************/
	// 设置无线组网密码。匹配令牌协议
	bool SetWiFi(const BinaryPair& args, Stream& result);

protected:
	virtual bool OnOpen();
	virtual void OnClose();

	// 引发数据到达事件
	virtual uint OnReceive(Buffer& bs, void* param);

private:
    OutputPort	_power;
    OutputPort	_rst;

	uint		_task;		// 调度任务
	ByteArray	_Buffer;	// 待处理数据包
	IPEndPoint	_Remote;	// 当前数据包远程地址

	//static void LoopTask(void* param);
	// 处理收到的数据包
	void Process();

	void*		_Expect;	// 等待内容

	// 多个硬件socket
	int* _sockets[5];

	// 分析+IPD接收数据。返回被用掉的字节数
	uint ParseReceive(const Buffer& bs);
	// 分析关键字。返回被用掉的字节数
	uint ParseReply(const Buffer& bs);
};

#endif
