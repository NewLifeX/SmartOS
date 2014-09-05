#include "Zigbee.h"

Zigbee::Zigbee(ITransport* port, Pin rst)
{
	assert_param(port);

	_port = port;
	
	_rst = NULL;
	if(rst != P0) 
	{
		_rst = new OutputPort(rst);
		Rest();
	}
}

Zigbee::~Zigbee()
{
	if(_port) delete _port;
	_port = NULL;
	
	if(_rst) delete _rst;
	_rst = NULL;
}
