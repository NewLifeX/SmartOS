
#include "ProxyFactory.h"

ProxyFactory * ProxyFactory::Current = nullptr;

/*
error code
0x01	没有端口
0x02	配置错误
0x03	打开出错
0x04	关闭出错
*/

ProxyFactory::ProxyFactory()
{
	Client = nullptr;
}

bool ProxyFactory::Open(TokenClient* client)
{
	if (!client)return false;
	Client = client;
	Client->Register("Proxy/GetConfig", &ProxyFactory::GetConfig,	this);
	Client->Register("Proxy/SetConfig", &ProxyFactory::SetConfig,	this);
	Client->Register("Proxy/Open",		&ProxyFactory::PortOpen,	this);
	Client->Register("Proxy/Close",		&ProxyFactory::PortClose,	this);
	Client->Register("Proxy/Write",		&ProxyFactory::Write,		this);
	Client->Register("Proxy/Read",		&ProxyFactory::Read,		this);
	Client->Register("Proxy/QueryPorts",&ProxyFactory::QueryPorts,	this);

	return true;
}

bool ProxyFactory::Register(cstring& name, Proxy* dev)
{
	Proxys.Add(name, dev);
	return true;
}

bool ProxyFactory::PortOpen(const BinaryPair& args, Stream& result)
{
	auto port = GetPort(args);

	auto ms = (MemoryStream&)result;
	BinaryPair rsbp(ms);
	if (!port)
	{
		rsbp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		if (!port->Open())
		{
			rsbp.Set("ErrorCode", (byte)0x03);
		}
	}

	return true;
}

bool ProxyFactory::PortClose(const BinaryPair& args, Stream& result)
{
	auto port = GetPort(args);

	auto ms = (MemoryStream&)result;
	BinaryPair rsbp(ms);
	if (!port)
	{
		rsbp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		if (!port->Close())
		{
			rsbp.Set("ErrorCode", (byte)0x04);
		}
	}

	return true;
}

bool ProxyFactory::Write(const BinaryPair& args, Stream& result)
{
	auto port = GetPort(args);

	auto ms = (MemoryStream&)result;
	BinaryPair rsbp(ms);
	if (!port)
	{
		rsbp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		auto bs = args.Get("Data");
		port->Write(bs);
	}

	return true;
}

bool ProxyFactory::Read(const BinaryPair& args, Stream& result)
{
	auto port = GetPort(args);

	auto ms = (MemoryStream&)result;
	BinaryPair rsbp(ms);
	if (!port)
	{
		rsbp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		auto bs = args.Get("Data");
		ByteArray rsbs;
		port->Read(rsbs, bs);

		rsbp.Set("Data", rsbs);
	}

	return true;
}

bool ProxyFactory::GetConfig(const BinaryPair& args, Stream& result)
{
	Proxy* port = GetPort(args);

	auto ms = (MemoryStream&)result;
	if (!port)
	{
		BinaryPair bp(ms);
		bp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		// String str;
		// port->GetConfig(str);	// 调用端口的函数处理内容
		// ms.Write(str);

		Dictionary<cstring, int> cfg;
		port->GetConfig(cfg);		// 调用端口的函数处理内容

		// 数据先写进缓冲区ms2
		MemoryStream ms2;
		auto name = cfg.Keys();
		auto value = cfg.Keys();

		for (int i = 0; i < cfg.Count(); i++)
		{
			ms2.Write(String(name[i]));
			ms2.Write('=');
			ms2.Write(String(value[i]));
			if (i < cfg.Count() - 1) ms2.Write('&');
		}
		// 然后组成名词对写进回复数据内去
		BinaryPair bp(ms);
		bp.Set("Config", Buffer(ms2.GetBuffer(), ms2.Position()));
	}

	return true;
}

bool ProxyFactory::SetConfig(const BinaryPair& args, Stream& result)
{
	Proxy* port = GetPort(args);

	auto ms = (MemoryStream&)result;
	if (!port)
	{
		BinaryPair bp(ms);
		bp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		Dictionary<cstring, int> config;		// Dictionary<cstring, cstring> 无法实例化  不知道哪出问题了

		String str;
		args.Get("config", str);			// 需要确认字段名称
		if (GetDic(str, config))
		{
			String str2;
			port->SetConfig(config, str2);	// 调用端口的函数处理内容
			ms.Write(str2);
		}
		else
		{
			BinaryPair bp(ms);
			bp.Set("ErrorCode", (byte)0x02);
		}
	}

	return true;
}

bool ProxyFactory::QueryPorts(const BinaryPair& args, Stream& result)
{
	MemoryStream ms;
	auto portnames = Proxys.Keys();
	for (int i = 0; i < Proxys.Count(); i++)
	{
		ms.Write(String(portnames[i]));
		if (i < Proxys.Count() - 1)ms.Write(',');
	}

	BinaryPair rsms(result);
	rsms.Set("Ports", Buffer(ms.GetBuffer(), ms.Position()));
	return true;
}

bool ProxyFactory::GetDic(String& str, Dictionary<cstring, int>& dic)
{
	int start = 0;
	int end = 0;

	char* name;
	char* data;
	while (1)
	{
		name = nullptr;
		data = nullptr;

		end = str.IndexOf('=', start);					// 找到 = 表示一个参数名的结束
		if (end > start && end < str.Length())	// 需要限制很多
		{
			cstring name = str.GetBuffer() + start;		// 拿到 name
			str[end] = '\0';
			start = end + 1;
		}
		else
		{
			if (!end)									// 没有 = 表示没有参数名了
				return false;
			else
				return true;
		}

		end = str.IndexOf('&', start);					// 找到 & 表示一个参数的结束
		if (!end)
			end = str.Length();
		else
			str[end] = '\0';
		if (end > start)
		{
			cstring data = str.GetBuffer() + start;		// 拿到 data
			start = end + 1;
		}

		dic.Add(name, String(data).ToInt());		// Dictionary<cstring, cstring> 无法实例化  不知道哪出问题了
	}
}

Proxy* ProxyFactory::GetPort(const BinaryPair& args)
{
	String Name;
	args.Get("Port", Name);
	if (!Name.Length())return nullptr;

	Proxy* port;
	Proxys.TryGetValue(Name.GetBuffer(), port);

	return port;
}

ProxyFactory* ProxyFactory::Create()
{
	if (!Current)Current = new ProxyFactory();
	return Current;
}
