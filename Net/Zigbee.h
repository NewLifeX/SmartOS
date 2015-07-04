#ifndef __Zigbee_H__
#define __Zigbee_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"

// Zigbee协议
// 主站发送所有从站收到，从站发送只有主站收到
class Zigbee : public ITransport
{
private:
	ITransport*	_port;
	OutputPort	_rst;

public:
	Zigbee();
	Zigbee(ITransport* port, Pin rst = P0);
	virtual ~Zigbee();
	void Init(ITransport* port, Pin rst = P0);

	// 注册回调函数
	virtual void Register(TransportHandler handler, void* param = NULL);

	virtual void Reset(void);

	virtual string ToString() { return "Zigbee"; }

protected:
	virtual bool OnOpen() { return _port->Open(); }
    virtual void OnClose() { _port->Close(); }

    virtual bool OnWrite(const byte* buf, uint len) { return _port->Write(buf, len); }
	virtual uint OnRead(byte* buf, uint len) { return _port->Read(buf, len); }

	static uint OnPortReceive(ITransport* sender, byte* buf, uint len, void* param);
};

#endif
