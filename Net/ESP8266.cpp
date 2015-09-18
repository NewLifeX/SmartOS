#include "ESP8266.h"

ESP8266::ESP8266(ITransport* port, Pin rst)
{
    assert_param(port);

	Set(port);
    
	if(rst != P0) _rst.Set(rst);
}

bool ESP8266::OnOpen()
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

void ESP8266::OnClose()
{
	_rst.Close();

	PackPort::OnClose();
}
