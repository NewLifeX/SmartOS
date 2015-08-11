#include "ESP8266.h"

ESP8266::ESP8266(ITransport* port, Pin rst)
{
    assert_param(port);

    _port = port;
    
    _rst = NULL;
    if(rst != P0) _rst = new OutputPort(rst);
}

ESP8266::~ESP8266()
{
    if(_port) delete _port;
    _port = NULL;
    
    if(_rst) delete _rst;
    _rst = NULL;
}
