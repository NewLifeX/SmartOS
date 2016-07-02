#include "Zigbee.h"

Zigbee::Zigbee()
{
	Set(nullptr);
}

Zigbee::Zigbee(ITransport* port, Pin rst)
{
	Init(port);
}

void Zigbee::Init(ITransport* port, Pin rst)
{
	Set(port);
	MaxSize	= 64;

	if(rst != P0) _rst.Set(rst);
}

bool Zigbee::OnOpen()
{
	if(!PackPort::OnOpen()) return false;

	if(!_rst.Empty())
	{
		_rst.Open();

		_rst = true;
		Sys.Delay(100);
		_rst = false;
		Sys.Delay(100);
		_rst = true;
	}

	return true;
}

void Zigbee::OnClose()
{
	_rst.Close();

	PackPort::OnClose();
}
