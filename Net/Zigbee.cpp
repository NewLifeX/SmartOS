#include "Zigbee.h"

Zigbee::Zigbee() { }

Zigbee::Zigbee(ITransport* port, Pin rst)
{
	Init(port);
}

void Zigbee::Init(ITransport* port, Pin rst)
{
	assert_ptr(port);

	Set(port);

	if(rst != P0)
	{
		_rst.Set(rst).Open();
		Reset();
	}
}

void Zigbee::Reset(void)
{
	if(_rst.Empty()) return;

	_rst = true;
	Sys.Delay(100);
	_rst = false;
	Sys.Delay(100);
	_rst = true;
}
