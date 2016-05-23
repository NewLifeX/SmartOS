#ifndef __Esp8266_H__
#define __Esp8266_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"
#include "Net\Net.h"
#include "Message\DataStore.h"


extern void EspTest(void * param);

// 最好打开 Soket 前 不注册中断，以免AT指令乱入到中断里面去  然后信息不对称
// 安信可 ESP8266  模块固件版本 v1.3.0.2
class Esp8266 : public PackPort, public ISocketHost
{
public:
	IDataPort*	Led;	// 指示灯
	Action		NetReady;	// 网络准备就绪

    Esp8266(ITransport* port, Pin power = P0, Pin rst = P0);

	void OpenAsync();
	virtual void Config();

	//virtual const String ToString() const { return String("Esp8266"); }
	virtual ISocket* CreateSocket(ProtocolType type);

	// 基础AT指令
	bool Test();
	bool Reset();
	String GetVersion();
	bool Sleep(uint ms);

	enum class Modes
	{
		Unknown = 0,
		Station = 1,
		Ap = 2,
		Both = 3,
	};
	
	Modes GetMode();
	bool SetMode(Modes mode);

	bool JoinAP(const String& ssid, const String& pwd);
	bool UnJoinAP();
	bool AutoConn(bool enable);

	// 发送指令
	String Send(const String& cmd, const String& expect, uint msTimeout = 1000);
	bool SendCmd(const String& cmd);
	bool SendCmd(const String& cmd, const String& expect, uint msTimeout = 1000, int times = 1);
	bool WaitForCmd(const String& expect, uint msTimeout);

protected:
	virtual bool OnOpen();
	virtual void OnClose();

	// 引发数据到达事件
	virtual uint OnReceive(Buffer& bs, void* param);

private:
    OutputPort	_power;
    OutputPort	_rst;
	String*		_Response;	// 响应内容
	
	Modes Mode = Modes::Unknown;
	// 数据回显  Write 数据被发回
	bool BackShow = false;
};

#endif
