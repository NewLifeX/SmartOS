
#include "Proxy.h"
#include "Message/ProxyFactory.h"


Proxy::Proxy()
{
	Cache		= nullptr;
	CacheSize	= 0;
	TimeStamp	= 0;
	Stamp		= false;
	UploadTaskId= 0;
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

void Proxy::UploadTask()
{
	auto  fac = ProxyFactory::Current;
	if (Cache->Position() && fac)
	{
		Buffer buf(Cache->GetBuffer(), Cache->Position());
		fac->Upload(*this, buf);
	}
	Cache->SetPosition(0);
}

bool Proxy::Upload(Buffer& data)
{
	if (!UploadTaskId)UploadTaskId = Sys.AddTask(&Proxy::UploadTask, this, -1, -1, "Proxy::Upload");

	// 没有Cache则直接发送
	BinaryPair bp(*Cache);
	if (Stamp)bp.Set("Stamp", Sys.Ms());
	bp.Set("Data", data);
	// CacheSize 为0时立马发送   ||   Cache 满立马发送
	if ((UploadTaskId && !CacheSize)
		||(CacheSize && Cache->Position()>CacheSize))Sys.SetTask(UploadTaskId, true, 0);
	/*
	不知道怎么在同一个包里存放多个数据，暂时没写缓存问题的东西
	*/
	return true;
}

void Proxy::AutoTask()
{
	OnAutoTask();
}

bool Proxy::GetConfig(ProxyConfig& cfg)
{
	return true;
}

/************************************************************************/

ComProxy::ComProxy(COM com) :port(com)
{
	// port.ToStr(Name);
	Name = port.Name;
	String name(Name);
	name.Show(true);
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
// 串口没有WriteRead函数
int ComProxy::Read(Buffer& data, Buffer& input)
{
	port.Write(input);
	if (port.Read(data) > 0)return true;
	return false;
}
