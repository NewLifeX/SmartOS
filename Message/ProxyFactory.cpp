
#include "ProxyFactory.h"

ProxyFactory * ProxyFactory::Current = nullptr;


ProxyFactory::ProxyFactory()
{
	Client = nullptr;
}

bool ProxyFactory::Open(TokenClient* client)
{
	if (!client)return false;
	Client = client;
	Client->Register("Proxy/GetConfig", &ProxyFactory::GetConfig, this);
	Client->Register("Proxy/SetConfig", &ProxyFactory::SetConfig, this);
	return true;
}

bool ProxyFactory::Register(cstring& name, Proxy* dev)
{
	Proxys.Add(name, dev);
	return true;
}

bool ProxyFactory::GetConfig(const BinaryPair& args, Stream& result)
{
	String Name;
	args.Get("name", Name);
	Proxy* port;
	Proxys.TryGetValue(Name.GetBuffer(), port);

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

		Dictionary<cstring, cstring> cfg;
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
		bp.Set("config", Buffer(ms2.GetBuffer(), ms2.Position()));
	}

	return true;
}

bool ProxyFactory::SetConfig(const BinaryPair& args, Stream& result)
{
	String Name;
	args.Get("name", Name);
	Proxy* port;
	Proxys.TryGetValue(Name.GetBuffer(), port);

	auto ms = (MemoryStream&)result;
	if (!port)
	{
		BinaryPair bp(ms);
		bp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		Dictionary<cstring, char* > config;		// Dictionary<cstring, cstring> 无法实例化  不知道哪出问题了

		String str;
		args.Get("config", str);			// 需要确认字段名称
		if (GetDic(str, config))
		{
			String str2;
			port->SetConfig((Dictionary<cstring, cstring> &)config, str2);	// 调用端口的函数处理内容
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
	rsms.Set("ports", Buffer(ms.GetBuffer(), ms.Position()));
	return true;
}

bool ProxyFactory::GetDic(String& str, Dictionary<cstring, char*>& dic)
{
	int start = 0;
	int end = 0;

	while (1)
	{
		char* name = nullptr;
		char* data = nullptr;

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

		dic.Add(name, data);		// Dictionary<cstring, cstring> 无法实例化  不知道哪出问题了
	}
}

ProxyFactory* ProxyFactory::Create()
{
	if (!Current)Current = new ProxyFactory();
	return Current;
}

