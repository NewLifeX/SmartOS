#ifndef _NetUri_H_
#define _NetUri_H_

#include "IPAddress.h"
#include "IPEndPoint.h"

// 协议类型
enum class NetType
{
	Unknown	= 0,
	Tcp		= 6,
	Udp		= 17,
	Http	= 80,
};

// 网络资源
class NetUri
{
public:
	IPAddress	Address;	// 地址
	ushort		Port;		// 端口
	NetType		Type;		// 协议类型
	String		Host;		// 远程地址，字符串格式，可能是IP字符串

	NetUri();
	NetUri(const String& uri);
	NetUri(NetType type, const IPAddress& addr, ushort port);
	NetUri(NetType type, const String& host, ushort port);

	bool IsTcp() const { return Type == NetType::Tcp; }
	bool IsUdp() const { return Type == NetType::Udp; }
	
	String ToString() const;
};

#endif
