#include "Ethernet.h"

void IEthernetAdapter::Register(DataHandler handler, void* param)
{
	_handler = handler;
	_param = param;
}

void IEthernetAdapter::OnReceive(byte* buf, uint len)
{
	if(!_handler) return;

	_handler(_param, buf, len);
}
