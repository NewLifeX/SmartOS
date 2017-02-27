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
	if(uri.CompareTo("Tcp", 3, true) == 0)
		Type = NetType::Tcp;
	else if(uri.CompareTo("Udp", 3, true) == 0)
		Type = NetType::Udp;
	else if(uri.CompareTo("Http", 4, true) == 0)
		Type = NetType::Http;

	int p	= uri.LastIndexOf("/");
	int end	= uri.LastIndexOf(":");
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

String NetUri::ToString() const
{
	String str;

	switch(Type)
	{
		case NetType::Tcp:	str	+= "Tcp"; break;
		case NetType::Udp:	str	+= "Udp"; break;
		case NetType::Http:	str	+= "Http"; break;
		default: break;
	}

	str	+= "://";

	if(Host)
		str	+= Host;
	else
		str	+= Address;

	str	+= ":";

	str	+= Port;

	return str;
}
