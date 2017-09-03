#include "Net\IPEndPoint.h"

#include "TokenConfig.h"

TokenConfig* TokenConfig::Current	= nullptr;

TokenConfig::TokenConfig() : ConfigBase()
{
	_Name	 = "TokenCf";
	_Start	 = &Length;
	_End	 = &TagEnd;
	Init();
}

void TokenConfig::Init()
{
	ConfigBase::Init();

	Length		= Size();

	SoftVer		= Sys.Ver;
	PingTime	= 10;
}

void TokenConfig::Show() const
{
#if DEBUG
	debug_printf("TokenConfig::令牌配置：\r\n");

	debug_printf("\t端口: %d \r\n", Port);
	debug_printf("\t服务: %s \r\n", _Server);
	debug_printf("\t登录: %s \r\n", _User);
	debug_printf("\t密码: %s \r\n", _Pass);
	debug_printf("\t令牌: %s \r\n", _Token);
#endif
}

TokenConfig* TokenConfig::Create(cstring server, ushort port)
{
	static TokenConfig tc;
	if(!Current)
	{
		TokenConfig::Current = &tc;
		tc.Init();
		tc.Load();

		auto uri = tc.Uri();
		
		// 默认 UDP 不允许 unknown
		if(uri.Type == NetType::Unknown) uri.Type = NetType::Udp;

		// 如果服务器配置为空，则使用外部地址
		auto svr = tc.Server();
		if (!svr)
		{
			svr = server;
			tc.Port = port;

			tc.Save();
		}
		
		tc.Show();
	}

	return &tc;
}
