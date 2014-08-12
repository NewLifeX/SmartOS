#ifndef __Tcp_H__
#define __Tcp_H__

#include "Sys.h"
#include "Net.h"
#include "Ethernet.h"

// Tcp协议
class Tcp
{
private:
	IEthernetAdapter* _adapter;

public:
	Tcp(IEthernetAdapter* adapter);
};

#endif
