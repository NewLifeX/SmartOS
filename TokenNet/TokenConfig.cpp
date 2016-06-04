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

	SoftVer		= Sys.Ver.Major;
	PingTime	= 10;
	//User[16] 	= '\0';
	//Key[15]	= '\0';

	Protocol	= ProtocolType::Udp;
}

void TokenConfig::Show() const
{
#if DEBUG
	debug_printf("TokenConfig::令牌配置：\r\n");

	debug_printf("\t协议: %s ,%d\r\n", Protocol == ProtocolType::Udp ? "UDP" : Protocol == ProtocolType::Tcp ? "TCP" : "", Protocol);
	debug_printf("\t端口: %d \r\n", Port);

	debug_printf("\t远程: ");
	IPEndPoint ep2(IPAddress(ServerIP), ServerPort);
	ep2.Show(true);
	debug_printf("\t服务: %s \r\n", _Server);
	debug_printf("\t厂商: %s \r\n", _Vendor);
	debug_printf("\t登录: %s \r\n", _User);
	debug_printf("\t密码: %s \r\n", _Pass);
#endif
}

TokenConfig* TokenConfig::Create(cstring vendor, ProtocolType protocol, ushort sport, ushort port)
{
	static TokenConfig tc;
	if(!Current)
	{
		TokenConfig::Current = &tc;
		tc.Init();
		tc.Protocol	= protocol;
		tc.Load();

		// 默认 UDP 不允许 unknown
		if(tc.Protocol == 0x00) tc.Protocol = ProtocolType::Udp;

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
