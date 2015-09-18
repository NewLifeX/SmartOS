#ifndef __ESP8266_H__
#define __ESP8266_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"

// Zigbee协议
// 主站发送所有从站收到，从站发送只有主站收到
class ESP8266 : public PackPort
{
private:
    OutputPort	_rst;

public:
    ESP8266(ITransport* port, Pin rst = P0);

	virtual string ToString() { return "ESP8266"; }

protected:
	virtual bool OnOpen();
	virtual void OnClose();
};

#endif
