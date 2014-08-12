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
	// 获取负载数据指针。外部可以直接填充数据
	virtual byte* GetPayload() = 0;
	virtual void Send(IP_TYPE type, uint len) = 0;
	void Register(IP_TYPE type, DataHandler handler, void* param = NULL);

protected:
	virtual void OnReceive(byte* buf, uint len) = 0;
};

// 以太网
class Ethernet
{
private:
	IEthernetAdapter* adapter;

public:
};

#endif
