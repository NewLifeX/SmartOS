#ifndef __Ethernet_H__
#define __Ethernet_H__

#include "Sys.h"
#include "Net.h"

// 网络适配器
class IEthernetAdapter
{
private:
	DataHandler _handler;
	void* _param;

public:
	virtual void Send(byte* buf, uint len) = 0;
	void Register(DataHandler handler, void* param = NULL);

protected:
	virtual void OnReceive(byte* buf, uint len);
};

// 以太网
class Ethernet
{
private:
	IEthernetAdapter* adapter;

public:
};

#endif
