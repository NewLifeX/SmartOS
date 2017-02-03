#ifndef _Socket_H_
#define _Socket_H_

#include "IPAddress.h"
#include "IPEndPoint.h"
#include "MacAddress.h"
#include "NetUri.h"
#include "NetworkInterface.h"

#include "Core\Delegate.h"

class NetworkInterface;

// Socket接口
class Socket
{
public:
	NetworkInterface*	Host;	// 主机
	NetType	Protocol;	// 协议类型

	IPEndPoint	Local;	// 本地地址。包含本地局域网IP地址，实际监听的端口，从1024开始累加
	IPEndPoint	Remote;	// 远程地址
	String		Server;	// 远程地址，字符串格式，可能是IP字符串

	// 加上虚析构函数，因为应用层可能要释放该接口
	virtual ~Socket() { }

	//// 应用配置，修改远程地址和端口
	//virtual bool Change(const String& remote, ushort port) { return false; };

	// 发送数据
	virtual bool Send(const Buffer& bs) = 0;
	virtual bool SendTo(const Buffer& bs, const IPEndPoint& remote) { return Send(bs); }
	// 接收数据
	virtual uint Receive(Buffer& bs) = 0;
};

#endif
