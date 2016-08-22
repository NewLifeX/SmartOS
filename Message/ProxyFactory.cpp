#include "ProxyFactory.h"
#include "Message\BinaryPair.h"

ProxyFactory * ProxyFactory::Current = nullptr;

/*
error code
0x01	没有端口
0x02	配置错误
0x03	打开出错
0x04	关闭出错
*/

ProxyFactory::ProxyFactory():Proxys(String::Compare)
{
	debug_printf("创建 ProxyFac\r\n");
	Client = nullptr;
}

bool ProxyFactory::Open(TokenClient* client)
{
	debug_printf("ProxyFac Open");
	if (!client)return false;

	// debug_printf("  Register");
	Client = client;
	Client->Register("Proxy/GetConfig", &ProxyFactory::GetConfig,	this);
	Client->Register("Proxy/SetConfig", &ProxyFactory::SetConfig,	this);
	Client->Register("Proxy/Open",		&ProxyFactory::PortOpen,	this);
	Client->Register("Proxy/Close",		&ProxyFactory::PortClose,	this);
	Client->Register("Proxy/Write",		&ProxyFactory::Write,		this);
	Client->Register("Proxy/Read",		&ProxyFactory::Read,		this);
	Client->Register("Proxy/QueryPorts",&ProxyFactory::QueryPorts,	this);
	debug_printf("\r\n");
	return true;
}

bool ProxyFactory::Register(Proxy* dev)
{
	// debug_printf("ProxyFac RegPort");
	String name = dev->Name;
	name.Show(true);
	Proxys.Add(dev->Name, dev);
	return true;
}

bool ProxyFactory::PortOpen(const Pair& args, Stream& result)
{
	debug_printf("ProxyFac PortOpen\r\n");
	auto port = GetPort(args);

	auto ms = (MemoryStream&)result;
	BinaryPair rsbp(ms);
	if (!port)
	{
		debug_printf("没有此Port\r\n");
		rsbp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		if (port->Open())
		{
			debug_printf("Open ");
			String name(port->Name);
			name.Show();
			debug_printf("  OK\r\n");
			result.Write((byte)1);
		}
		else
		{
			debug_printf("Open Error\r\n");
			rsbp.Set("ErrorCode", (byte)0x03);
		}
	}

	return true;
}

bool ProxyFactory::PortClose(const Pair& args, Stream& result)
{
	debug_printf("ProxyFac PortClose\r\n");
	auto port = GetPort(args);

	auto ms = (MemoryStream&)result;
	BinaryPair rsbp(ms);
	if (!port)
	{
		debug_printf("没有此Port\r\n");
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

bool ProxyFactory::Write(const Pair& args, Stream& result)
{
	// debug_printf("ProxyFac Write\r\n");
	auto port = GetPort(args);

	auto ms = (MemoryStream&)result;
	BinaryPair rsbp(ms);
	if (!port)
	{
		debug_printf("没有此Port\r\n");
		rsbp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		auto bs = args.Get("Data");
		if(!bs.Length()) bs = args.Get("data");

		debug_printf("Write len: %d  data:",bs.Length());
		bs.Show(true);

		int len = port->Write(bs);
		result.Write(len);
	}

	return true;
}

bool ProxyFactory::Read(const Pair& args, Stream& result)
{
	debug_printf("ProxyFac Read\r\n");
	auto port = GetPort(args);

	auto ms = (MemoryStream&)result;
	BinaryPair rsbp(ms);
	if (!port)
	{
		debug_printf("没有此Port\r\n");
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

bool ProxyFactory::GetConfig(const Pair& args, Stream& result)
{
	// debug_printf("ProxyFac GetConfig\r\n");
	Proxy* port = GetPort(args);

	auto ms = (MemoryStream&)result;
	if (!port)
	{
		BinaryPair bp(ms);
		debug_printf("没有此Port\r\n");
		bp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		// String str;
		// port->GetConfig(str);	// 调用端口的函数处理内容
		// ms.Write(str);

		Dictionary<char *, int> cfg;
		port->GetConfig(cfg);		// 调用端口的函数处理内容

		if (cfg.Count() < 1)
		{
			result.Write((byte)0);
			return false;
		}

		// 数据先写进缓冲区ms2
		MemoryStream ms2;
		auto& name = cfg.Keys();
		auto& value = cfg.Values();
		
		// debug_printf("cfg count : %d value count : %d\t\t", name.Count(), value.Count());
		String str;

		debug_printf("config:\r\n");
		for (int i = 0; i < cfg.Count(); i++)
		{
			debug_printf("%s     %d\r\n",name[i],value[i]);
			str = str + name[i] + '=' + value[i];
			if (i < cfg.Count() - 1)str = str + '&';
		}
		str.Show(true);
		//ms.Write(str);
		result.Write(str);

		// for (int i = 0; i < cfg.Count(); i++)
		// {
		// 	ms2.Write(String(name[i]));
		// 	ms2.Write('=');
		// 	ms2.Write(String(value[i]));
		// 	if (i < cfg.Count() - 1) ms2.Write('&');
		// }
		// // 然后组成名词对写进回复数据内去
		// BinaryPair bp(ms);
		// bp.Set("Config", Buffer(ms2.GetBuffer(), ms2.Position()));
	}

	return true;
}

bool ProxyFactory::SetConfig(const Pair& args, Stream& result)
{
	// debug_printf("ProxyFac SetConfig\r\n");
	Proxy* port = GetPort(args);

	auto ms = (MemoryStream&)result;
	if (!port)
	{
		BinaryPair bp(ms);
		debug_printf("没有此Port\r\n");
		bp.Set("ErrorCode", (byte)0x01);
	}
	else
	{
		Dictionary<cstring, int> config(String::Compare);		// Dictionary<cstring, cstring> 无法实例化  不知道哪出问题了

		String str;
		args.Get("config", str);			// 需要确认字段名称
		if (GetDic(str, config))
		{
			String str2;
			port->SetConfig(config, str2);	// 调用端口的函数处理内容
			ms.Write(str2);
			result.Write((byte)1);
		}
		else
		{
			BinaryPair bp(ms);
			bp.Set("ErrorCode", (byte)0x02);
		}
	}

	return true;
}

bool ProxyFactory::QueryPorts(const Pair& args, Stream& result)
{
	// debug_printf("ProxyFac QueryPorts\r\n");
	auto portnames = Proxys.Keys();
	String name;
	for (int i = 0; i < Proxys.Count(); i++)
	{
		name = name + portnames[i];
		if (i < Proxys.Count() - 1)name = name + ',';
	}
	name.Show(true);

	result.Write(name);

	return true;
}

bool ProxyFactory::Upload(Proxy& port, Buffer& data)
{
	if (!Client)return false;
	MemoryStream ms;
	BinaryPair bp(ms);
	bp.Set("Port", port.Name);
	bp.Set("Data", data);
	Client->Invoke("Proxy/Upload", Buffer(ms.GetBuffer(),ms.Position()));
	return true;
}

bool ProxyFactory::UpOpen(Proxy& port)
{
	if (!Client)return false;
	MemoryStream ms;
	BinaryPair bp(ms);
	bp.Set("Port", port.Name);
	bp.Set("Open", (byte)0x01);

	Client->Invoke("Proxy/Open", Buffer(ms.GetBuffer(), ms.Position()));
	return true;
}

bool ProxyFactory::AutoStart()
{
	if (!Client->Opened)
	{
		debug_printf("请在TokenClient Open后再执行\r\n");
		return false;
	}
	auto ports = Proxys.Values();
	for (int i = 0; i < ports.Count(); i++)
	{
		ports[i]->LoadConfig();
		if (ports[i]->AutoStart)ports[i]->Open();
	}
	return true;
}

bool ProxyFactory::GetDic(String& str, Dictionary<cstring, int>& dic)
{
	int start = 0;
	int end = 0;
	debug_printf("GetDic data: ");
	str.Show(true);

	char* name;
	char* data;
	while (1)
	{
		name = nullptr;
		data = nullptr;

		end = str.IndexOf('=', start);					// 找到 = 表示一个参数名的结束
		if (end > start && end < str.Length())	// 需要限制很多
		{
			name = (char*)str.GetBuffer() + start;		// 拿到 name
			if (str[end + 1] == '&')	// =& 直接忽略
			{
				start = end + 1;
				continue;			
			}
			str[end] = '\0';
			start = end + 1;
		}
		else
		{
			if (!end)									// 没有 = 表示没有参数名了
			{
				debug_printf("dic Count = %d\r\n",dic.Count());
				return false;
			}
			else
			{
				debug_printf("dic Count = %d\r\n", dic.Count());
				return true;
			}
		}

		end = str.IndexOf('&', start);					// 找到 & 表示一个参数的结束
		if (!end)
			end = str.Length();
		else
			str[end] = '\0';
		if (end > start)
		{
			data = (char*)str.GetBuffer() + start;		// 拿到 data
			start = end + 1;
		}
		int value = String(data).ToInt();
		dic.Add(name, value);// String(data).ToInt());		// Dictionary<cstring, cstring> 无法实例化  不知道哪出问题了
		debug_printf("%s\t\t%d\r\n", name, value);
	}
}

Proxy* ProxyFactory::GetPort(const Pair& args)
{
	String Name;

	args.Get("port", Name);
	if (!Name.Length())
	{
		args.Get("Port", Name);
		if (!Name.Length())
		{
			debug_printf("GetPort GetName Error\r\n");
			return nullptr;
		}
	}

	debug_printf("GetPort Name = ");
	Name.Show(true);

	Proxy* port;
	Proxys.TryGetValue(Name.GetBuffer(), port);
	
	return port;
}

ProxyFactory* ProxyFactory::Create()
{
	if (!Current)Current = new ProxyFactory();
	return Current;
}
