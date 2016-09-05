#include "NetUri.h"
#include "Config.h"

/******************************** NetUri ********************************/

NetUri::NetUri()
{
	Type	= NetType::Unknown;
	Address	= IPAddress::Any();
	Port	= 0;
}

NetUri::NetUri(const String& uri) : NetUri()
{
	if (uri.Contains("tcp") || uri.Contains("Tcp") || uri.Contains("TCP"))
		Type = NetType::Tcp;
	else
		Type = NetType::Udp;

	int p		= uri.LastIndexOf("/");
	int end		= uri.LastIndexOf(":");
	auto hlent = end - p - 1;
	if (hlent > 0 && p > 0)
	{
		Host = uri.Substring(p+1, hlent);
	}

	String port;
	auto plent = uri.Length() - end-1;

	if (plent > 0 && end > 0)
	{
		port = uri.Substring(end+1, plent);
		Port = port.ToInt();
	}
}

NetUri::NetUri(NetType type, const IPAddress& addr, ushort port)
{
	Type	= type;
	Address	= addr;
	Port	= port;
}

NetUri::NetUri(NetType type, const String& host, ushort port)
{
	Type	= type;
	Address	= IPAddress::Any();
	Host	= host;
	Port	= port;
}

String& NetUri::ToStr(String& str) const
{
	if(IsTcp())
		str	+= "Tcp";
	else if(IsUdp())
		str	+= "Udp";

	str	+= "://";

	if(Host)
		str	+= Host;
	else
		str	+= Address;

	str	+= ":";

	str	+= Port;

	return str;
}
