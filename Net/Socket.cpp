#include "Socket.h"
#include "Config.h"

#include "NetworkInterface.h"

#define NET_DEBUG DEBUG
//#define NET_DEBUG 0
#if NET_DEBUG
	#define net_printf debug_printf
#else
	#define net_printf(format, ...)
#endif

Socket* Socket::CreateClient(const NetUri& uri)
{
	auto& list	= NetworkInterface::All;
	for(int i=0; i < list.Count(); i++)
	{
		auto ni	= list[i];
		if(ni->Active())
		{
			auto socket	= ni->CreateSocket(uri.Type);
			if(socket)
			{
				socket->Local.Address	= uri.Address;
				socket->Local.Port		= uri.Port;

				return socket;
			}
		}
	}

	return nullptr;
}

Socket* Socket::CreateRemote(const NetUri& uri)
{
	auto& list	= NetworkInterface::All;
	for(int i=0; i < list.Count(); i++)
	{
		auto ni	= list[i];
		if(ni && ni->Active())
		{
			auto socket	= ni->CreateSocket(uri.Type);
			if(socket)
			{
				socket->Local.Address	= ni->IP;
				socket->Remote.Address	= uri.Address;
				socket->Remote.Port		= uri.Port;
				socket->Server			= uri.Host;

				return socket;
			}
		}
	}

	return nullptr;
}
