#include "Udp.h"

Udp::Udp(IEthernetAdapter* adapter, ushort bufferSize)
{
	_adapter = adapter;
	_Buffer = new byte[bufferSize];
	_header = (UDP_HEADER*)_Buffer;
}

Udp::Udp()
{
	_adapter = NULL;
	Register(NULL);
	
	if(_Buffer) delete _Buffer;
	_Buffer = NULL
}

// 向目标地址发送数据
void Udp::Send(byte* buf, uint len, byte[4] remoteip, ushort remoteport)
{
	_adapter->Send(IP_UDP, buf, len);
}

void Udp::Listen()
{
	
}
