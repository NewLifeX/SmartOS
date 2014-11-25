#ifndef __Slave_H__
#define __Slave_H__

#include "Sys.h"

// Modbus从机
class Slave
{
private:
	ITransport*	_port;	// 传输口

public:
	byte	Address;	// 地址
	
	Slave(ITransport* port);
	~Slave();

private:
	static void OnReceive(ITransport* transport, byte* buf, uint len, void* param);

	// 分发处理消息。返回值决定是否响应
	bool Dispatch(Modbus& entity);
};

#endif
