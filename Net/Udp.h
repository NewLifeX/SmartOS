#ifndef __Udp_H__
#define __Udp_H__

#include "Sys.h"
#include "Net.h"
#include "Ethernet.h"

// UDP协议
class Udp
{
private:
	IEthernetAdapter* adapter;
	byte* _Buffer;
	UDP_HEADER* _header;

public:
	Udp(IEthernetAdapter* adapter, ushort bufferSize = 1400);
	virtual ~Udp();
	
	ushort Port;	// 本地端口。0表示监听所有端口
	
	void Listen();
	
	// 向目标地址发送数据，数据必须提前填充到负载里面
	void Send(uint len, byte[4] remoteip, ushort remoteport);
	// 获取负载数据指针。外部可以直接填充数据
	byte* GetPayload() { return _Buffer + sizeof(UDP_HEADER); }
	// 注册回调函数
	void Register(DatHandler* handler, void* param = NULL);
private:
	DataHandler	_handler;
	void*		_Param;
};

#endif
