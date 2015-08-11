#ifndef __ESP8266_H__
#define __ESP8266_H__

#include "Sys.h"
#include "Port.h"
#include "Net\ITransport.h"

// Zigbee协议
// 主站发送所有从站收到，从站发送只有主站收到
class ESP8266 : public ITransport
{
private:
    ITransport* _port;
    OutputPort* _rst;

public:
    ESP8266(ITransport* port, Pin rst = P0);
    virtual ~ESP8266();

    // 注册回调函数
    virtual void Register(TransportHandler handler, void* param = NULL)
    {
        ITransport::Register(handler, param);
        
        _port->Register(OnPortReceive, this);
    }

protected:
    virtual bool OnOpen() { return _port->Open(); }
    virtual void OnClose() { _port->Close(); }

    virtual bool OnWrite(const byte* buf, uint len) { return _port->Write(buf, len); }
    virtual uint OnRead(byte* buf, uint len) { return _port->Read(buf, len); }
    
    static uint OnPortReceive(ITransport* sender, byte* buf, uint len, void* param)
    {
        ESP8266 * zb = (ESP8266*)param;
        return zb->OnReceive(buf, len);
    }
};

#endif
