#include "NetUri.h"
#include "Config.h"

/******************************** NetUri ********************************/

NetUri::NetUri()
{
	Type	= NetType::Unknown;
	Address	= IPAddress::Any();
	Port	= 0;
}

/*NetUri::NetUri(const String& uri) : NetUri()
{
	int p	= uri.IndexOf("://");
	if(p)
	{

	}
}*/

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
