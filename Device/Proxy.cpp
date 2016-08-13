
#include "Proxy.h"


Proxy::Proxy()
{
	Cache		= nullptr;
	CacheSize	= 0;
	TimeStamp	= 0;
	Stamp		= false;
}

bool Proxy::Open()
{
	CacheSize = 256;
	Cache = new MemoryStream(CacheSize);
	OnOpen();
	return true;
}

bool Proxy::Close()
{
	delete Cache;
	OnClose();
	return true;
}

bool Proxy::Upload(Buffer& data)
{
	if (!Cache || !CacheSize)
	{
		// 没有Cache则直接发送
		MemoryStream ms;
		BinaryPair bp(ms);
		bp.Set("Port", name);

		if (Stamp)bp.Set("Stamp",Sys.Ms());
		bp.Set("Data", data);
		Buffer data2(ms.GetBuffer(), ms.Position());

		// auto * fac = ProxyFactory::Current;
		// fac->Client->Invoke("Proxy\Upload",data2);
	}
	return true;
}

