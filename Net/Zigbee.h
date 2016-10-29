#ifndef __Zigbee_H__
#define __Zigbee_H__

#include "Sys.h"
#include "Device\Port.h"
#include "Net\ITransport.h"

// Zigbee协议
// 主站发送所有从站收到，从站发送只有主站收到
class Zigbee : public PackPort
{
private:
	OutputPort	_rst;

public:
	Zigbee();
	Zigbee(ITransport* port, Pin rst = P0);
	void Init(ITransport* port, Pin rst = P0);

	//virtual const String ToString() const { return String("Zigbee"); }

protected:
	virtual bool OnOpen();
	virtual void OnClose();
};

#endif
