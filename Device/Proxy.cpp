
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
	if (!Cache)Cache = new MemoryStream(CacheSize);
	OnOpen();
	return true;
}

bool Proxy::Close()
{
	OnClose();
	delete Cache;
	return true;
}

bool Proxy::Upload(Buffer& data)
{
	if (!Cache || !CacheSize)
	{
		// 没有Cache则直接发送
		MemoryStream ms;
		BinaryPair bp(ms);
		bp.Set("Port", Name);

		if (Stamp)bp.Set("Stamp", Sys.Ms());
		bp.Set("Data", data);
		Buffer data2(ms.GetBuffer(), ms.Position());

		// auto * fac = ProxyFactory::Current;
		// fac->Client->Invoke("Proxy\Upload",data2);
	}
	return true;
}


/************************************************************************/


ComProxy::ComProxy(COM com) :port(com)
{
	// port.ToStr(Name);
	Name = port.Name;
}

bool ComProxy::OnOpen()
{
	return port.Open();
	// return true;
}

bool ComProxy::OnClose()
{
	port.Close();
	return true;
}

bool ComProxy::SetConfig(Dictionary<cstring, int>& config, String& str)
{
	int value;
	if (config.TryGetValue("baudRate", value))
	{
		port.SetBaudRate(value);
	}

	cstring ByteParam[] = { "parity","dataBits","stopBits" };
	byte*	ParamP[] = {&parity, &dataBits, &stopBits};
	bool haveChang = false;
	for (int i = 0; i < ArrayLength(ByteParam); i++)
	{
		value = 0;
		if (config.TryGetValue(ByteParam[i], value))
		{
			*(ParamP[i]) = value;
			haveChang = true;
		}
	}
	if (haveChang)port.Set(parity,  dataBits,  stopBits);

	return true;
}

bool ComProxy::GetConfig(Dictionary<cstring, int>& config)
{
	config.Add("baudRate",baudRate);

	config.Add("parity",  parity);
	config.Add("dataBits", dataBits);
	config.Add("stopBits", stopBits);

	return true;
}

int ComProxy::Write(Buffer& data)
{
	port.Write(data);
	return true;
}

int ComProxy::Read(Buffer& data, Buffer& input)
{
	port.Write(input);
	if (port.Read(data) > 0)return true;
	return false;
}
