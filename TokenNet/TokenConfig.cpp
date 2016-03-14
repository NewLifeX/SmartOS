#include "TokenConfig.h"
#include "Net\Net.h"
#include "Config.h"

TokenConfig* TokenConfig::Current	= nullptr;

TokenConfig::TokenConfig() : ConfigBase(),
	User(_User, ArrayLength(_User)),
	Pass(_Pass, ArrayLength(_Pass)),
	VisitToken(_VisitToken, ArrayLength(_VisitToken)),
	Server(_Server, ArrayLength(_Server)),
	Vendor(_Vendor, ArrayLength(_Vendor))
{
	_Name	 = "TokenCfg";
	_Start	 = &Length;
	_End	 = &TagEnd;
	Init();
}

void TokenConfig::Init()
{
	ConfigBase::Init();

	Length		= Size();
	ServerIP	= 0;

	SoftVer		= Sys.Version;
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
	debug_printf("\t服务: %s \r\n", Server.GetBuffer());
	debug_printf("\t厂商: %s \r\n", Vendor.GetBuffer());
	debug_printf("\t登录: %s \r\n", User.GetBuffer());
	debug_printf("\t密码: %s \r\n", Pass.GetBuffer());
#endif
}

TokenConfig* TokenConfig::Create(const char* vendor, byte protocol, ushort sport, ushort port)
{
	static TokenConfig tc;
	if(!Current)
	{
		TokenConfig::Current = &tc;
		tc.Init();
		tc.Load();

		bool rs = tc.New;
		if(!tc.Vendor)
		{
			tc.Vendor	= vendor;

			rs	= false;
		}

		if(!tc.Server)
		{
			tc.Server	= tc.Vendor;
			tc.ServerPort	= sport;
			tc.Port		= port;

			rs	= false;
		}
		if(!rs) tc.Save();

		tc.Show();
	}

	return &tc;
}
