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
	ServerIP	= 0;

	SoftVer		= Sys.Ver;
	PingTime	= 10;
	//User[16] 	= '\0';
	//Key[15]	= '\0';

	Protocol	= NetType::Udp;
}

NetUri	TokenConfig::Uri() const
{
	NetUri uri(Protocol, IPAddress(ServerIP), ServerPort);
	uri.Host	= _Server;

	return uri;
}

void TokenConfig::Show() const
{
#if DEBUG
	debug_printf("TokenConfig::令牌配置：\r\n");

	//debug_printf("\t协议: %s ,%d\r\n", Protocol == NetType::Udp ? "UDP" : Protocol == NetType::Tcp ? "TCP" : "", Protocol);
	debug_printf("\t端口: %d \r\n", Port);

	debug_printf("\t远程: ");
	NetUri uri(Protocol, IPAddress(ServerIP), ServerPort);
	uri.ToString().Show(true);
	debug_printf("\t服务: %s \r\n", _Server);
	debug_printf("\t厂商: %s \r\n", _Vendor);
	debug_printf("\t登录: %s \r\n", _User);
	debug_printf("\t密码: %s \r\n", _Pass);
	debug_printf("\t令牌: %s \r\n", _Token);
#endif
}

TokenConfig* TokenConfig::Create(cstring vendor, NetType protocol, ushort sport, ushort port)
{
	static TokenConfig tc;
	if(!Current)
	{
		TokenConfig::Current = &tc;
		tc.Init();
		tc.Protocol	= protocol;
		tc.Load();

		// 默认 UDP 不允许 unknown
		if(tc.Protocol == NetType::Unknown) tc.Protocol = NetType::Udp;

		bool rs = tc.New;
		auto vnd	= tc.Vendor();
		if(!vnd)
		{
			vnd	= vendor;

			rs	= false;
		}

		auto svr	= tc.Server();
		if(!svr)
		{
			svr	= vnd;
			tc.ServerPort	= sport;
			tc.Port		= port;

			rs	= false;
		}
		if(!rs) tc.Save();

		tc.Show();
	}

	return &tc;
}
