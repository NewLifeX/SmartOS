
#ifndef __ESP8266_H__
#define __ESP8266_H__

#include "Sys.h"
#include "SerialPort.h"
#include "Power.h"
#include "Time.h"
#include "Core\Stream.h"
#include "Net\ITransport.h"
#include "Net\Net.h"


extern void EspTest(void * param);

enum AiEspMode
{
	Unknown = 0,
	Station = 1,
	Ap = 2,
	Both = 3,
};

// 最好打开 Soket 前 不注册中断，以免AT指令乱入到中断里面去  然后信息不对称
// 安信可 ESP8266  模块固件版本 v1.3.0.2
class AiESP //: public Power, public ISocketHost
{
public:
	AiESP();
	AiESP(COM com, AiEspMode mode = Unknown);
	bool SetMode(AiEspMode mode);

	AiEspMode GetMode();
	// timeOutMs 为等待返回数据时间  为0表示不需要管返回
	void  Send(Buffer & dat);
	bool SendCmd(String &str,uint timeOutMs = 1000);

	int RevData(MemoryStream &ms, uint timeMs=5);

	bool JoinAP(String & ssid, String &pwd);
	bool UnJoinAP();
	bool AutoConn(bool enable);
	//private:
	SerialPort Port;
	AiEspMode Mode = AiEspMode::Unknown;
	// 数据回显  Write 数据被发回
	bool BackShow = false;
};

#endif
