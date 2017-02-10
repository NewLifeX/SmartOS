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
		auto socket	= list[i]->CreateClient(uri);
		if(socket) return socket;
	}

	return nullptr;
}

Socket* Socket::CreateRemote(const NetUri& uri)
{
	auto& list	= NetworkInterface::All;
	for(int i=0; i < list.Count(); i++)
	{
		auto socket	= list[i]->CreateRemote(uri);
		if(socket) return socket;
	}

	return nullptr;
}
