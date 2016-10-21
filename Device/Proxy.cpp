#include "Proxy.h"
#include "Message/ProxyFactory.h"
#include "Message/BinaryPair.h"

Proxy::Proxy()
{
	Cache		= nullptr;
	CacheSize	= 10;
	BufferSize	= 256;
	TimeStamp	= 0;
	EnableStamp	= false;
	UploadTaskId= 0;
	AutoStart	= false;
}

bool Proxy::Open()
{
	LoadConfig();
	if (!Cache)Cache = new MemoryStream(BufferSize);
	OnOpen();
	return true;
}

bool Proxy::Close()
{
	OnClose();
	delete Cache;
	return true;
}

bool Proxy::SetConfig(Dictionary<cstring, int>& config, String& str)
{
	int value;

	cstring const ByteParam[] = { "cache","buffersize" };
	int*	ParamP[] = { &CacheSize, &BufferSize };
	for (int i = 0; i < ArrayLength(ByteParam); i++)
	{
		value = 0;
		if (config.TryGetValue(ByteParam[i], value))
		{
			*(ParamP[i]) = value;
		}
	}

	cstring const ByteParam2[] = { "auto","timestamp" };
	bool*	ParamP2[] = { &AutoStart, &EnableStamp };
	for (int i = 0; i < ArrayLength(ByteParam2); i++)
	{
		value = 0;
		if (config.TryGetValue(ByteParam2[i], value))
		{
			*(ParamP2[i]) = value;
		}
	}

	OnSetConfig(config, str);
	SaveConfig();
	return true;
}

bool Proxy::GetConfig(Dictionary<cstring, int>& config)
{
	LoadConfig();

	config.Add("cache", CacheSize);
	config.Add("buffersize", BufferSize);

	config.Add("auto", AutoStart);
	config.Add("timestamp", EnableStamp);

	// debug_printf("基础配置条数%d\r\n",config.Count());
	OnGetConfig(config);

	debug_printf("一共%d跳配置",config.Count());

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
	if (EnableStamp)bp.Set("Stamp", Sys.Ms());
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

bool Proxy::LoadConfig()
{
	ProxyConfig cfg(Name);
	cfg.Load();

	CacheSize = cfg.CacheSize;
	BufferSize = cfg.BufferSize;
	EnableStamp = cfg.EnableStamp;
	AutoStart = cfg.AutoStart;

	Stream st(cfg.PortCfg, sizeof(cfg.PortCfg));
	OnGetConfig(st);

	return true;
}

void Proxy::SaveConfig()
{
	ProxyConfig cfg(Name);

	cfg.CacheSize	= CacheSize;
	cfg.EnableStamp	= EnableStamp;
	cfg.AutoStart	= AutoStart;
	cfg.BufferSize  = BufferSize;

	Stream st(cfg.PortCfg, sizeof(cfg.PortCfg));
	OnSetConfig(st);

	cfg.Save();
}


/************************************************************************/

ComProxy::ComProxy(COM com) :port(com)
{
	// port.ToStr(Name);
	Name = port.Name;
	String name(Name);
	name.Show(true);

	ProxyConfig cfg(Name);
	cfg.Load();
	if (cfg.New)		// 首次运行直接打开拿到默认配置后关闭
	{
		debug_printf("初次使用，打开端口获取默认配置:");
		port.Open();
		baudRate = port._baudRate;

		parity = port._parity;	// USART_Parity_No;
		dataBits = port._dataBits;	// USART_WordLength_8b;
		stopBits = port._stopBits;	// USART_StopBits_1;

		SaveConfig();
		port.Close();
	}
	// else
	// {
	// 	LoadConfig();
	// }
	// parity = 0x0000;	// USART_Parity_No;
	// dataBits = 0x0000;	// USART_WordLength_8b;
	// stopBits = 0x0000;	// USART_StopBits_1;
}

bool ComProxy::OnOpen()
{
	if (port.Open())
	{
		port.SetBaudRate(baudRate);
		port.Set(parity, dataBits, stopBits);
		port.Register(Dispatch, this);
		return true;
	}
	return false;
}

bool ComProxy::OnClose()
{
	port.Close();
	return true;
}

bool ComProxy::OnSetConfig(Dictionary<cstring, int>& config, String& str)
{
	int value;
	if (config.TryGetValue("baudrate", value))
	{
		port.SetBaudRate(value);
		baudRate = value;
	}

	cstring const ByteParam[] = { "parity","dataBits","stopBits" };
	ushort*	ParamP[] = {&parity, &dataBits, &stopBits};
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
	// SaveConfig();
	return true;
}

bool ComProxy::OnGetConfig(Dictionary<cstring, int>& config)
{
	config.Add("baudrate", baudRate);

	config.Add("parity", parity);
	config.Add("dataBits", dataBits);
	config.Add("stopBits", stopBits);

	return true;
}

int ComProxy::Write(Buffer& data)
{
	port.Write(data);
	return data.Length();
}

// 串口没有WriteRead函数
int ComProxy::Read(Buffer& data, Buffer& input)
{
	port.Write(input);
	if (port.Read(data) > 0)return true;
	return false;
}

uint ComProxy::Dispatch(ITransport* port, Buffer& bs, void* param, void* param2)
{
	// 串口不关心 param2
	auto& pro = *(ComProxy*)param;
	pro.Upload(bs);
	
	// auto fac = ProxyFactory::Current;	// 直接扔给上级
	// if (!fac)return 0;
	// fac->Upload(pro, bs);
	return 0;
}

bool ComProxy::OnGetConfig(Stream& cfg)
{
	parity = cfg.ReadUInt16();
	dataBits = cfg.ReadUInt16();
	stopBits = cfg.ReadUInt16();
	baudRate = cfg.ReadUInt32();
	return true;
}

bool ComProxy::OnSetConfig(Stream& cfg)
{
	cfg.Write(parity);
	cfg.Write(dataBits);
	cfg.Write(stopBits);
	cfg.Write(baudRate);
	return true;
}
