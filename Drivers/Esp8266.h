#ifndef __Esp8266_H__
#define __Esp8266_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"
#include "Net\Net.h"

// Zigbee协议
// 主站发送所有从站收到，从站发送只有主站收到
class Esp8266 : public PackPort, public ISocketHost
{
private:
    OutputPort	_rst;

public:
	IDataPort*	Led;	// 指示灯

    Esp8266(ITransport* port, Pin rst = P0);

	//bool Open();
	//bool Close();
	virtual void Config();

	//virtual const String ToString() const { return String("Esp8266"); }
	virtual ISocket* CreateSocket(ProtocolType type);

protected:
	virtual bool OnOpen();
	virtual void OnClose();
};

#endif
