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

    Esp8266(ITransport* port, Pin rst = P0);
	//Esp8266(COM com, AiEspMode mode = Unknown);

	virtual void Config();

	//virtual const String ToString() const { return String("Esp8266"); }
	virtual ISocket* CreateSocket(ProtocolType type);

	enum class Modes
	{
		Unknown = 0,
		Station = 1,
		Ap = 2,
		Both = 3,
	};

	Modes GetMode();
	bool SetMode(Modes mode);

	// timeOutMs 为等待返回数据时间  为0表示不需要管返回
	//void  Send(Buffer & dat);
	//bool SendCmd(String &str,uint timeOutMs = 1000);

	//int RevData(MemoryStream &ms, uint timeMs=5);

	bool JoinAP(const String& ssid, const String& pwd);
	bool UnJoinAP();
	bool AutoConn(bool enable);


protected:
	virtual bool OnOpen();
	virtual void OnClose();

	String Send(const String& str, uint msTimeout = 1000);
	bool SendCmd(const String& str, uint msTimeout = 1000, int times = 1);

	// 引发数据到达事件
	virtual uint OnReceive(Buffer& bs, void* param);

private:
    OutputPort	_rst;
	String*		_Response;	// 响应内容
	
	Modes Mode = Modes::Unknown;
	// 数据回显  Write 数据被发回
	bool BackShow = false;
};

#endif
