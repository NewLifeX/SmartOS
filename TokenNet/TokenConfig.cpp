#include "TokenConfig.h"
#include "Net\Net.h"
#include "Config.h"

TokenConfig* TokenConfig::Current	= NULL;

TokenConfig::TokenConfig() : ConfigBase()
{
	_Name	 = "TKCF";
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
	Name[16] = '\0';
	Key[16]	 = '\0';	

	Protocol	= 2;
}

void TokenConfig::Show() const
{
#if DEBUG
	debug_printf("TokenConfig::令牌配置：\r\n");

	debug_printf("\t协议: %s \r\n", Protocol == 2 ? "UDP" : "TCP");
	debug_printf("\t端口: %d \r\n", Port);

	debug_printf("\t远程: ");
	IPEndPoint ep2(IPAddress(ServerIP), ServerPort);
	ep2.Show(true);
	debug_printf("\t服务: %s \r\n", Server);
	debug_printf("\t厂商: %s \r\n", Vendor);
	debug_printf("\t登录: %s \r\n", Name);
	debug_printf("\t密码: %s \r\n", Key);
#endif
}

TokenConfig* TokenConfig::Create(const char* vendor, byte protocol, ushort sport, ushort port)
{
	static TokenConfig tc;
	if(!Current)
	{
		TokenConfig::Current = &tc;
		tc.Init();

		//strcpy(tc.Vendor, vendor);
		tc.Load();
		bool rs = tc.New;

		if(tc.Vendor[0] == 0)
		{
			strncpy(tc.Vendor, vendor, ArrayLength(tc.Vendor));

			rs	= false;
		}

		if(tc.Server[0] == 0)
		{
			strncpy(tc.Server, tc.Vendor, ArrayLength(tc.Server));

			//tc.ServerIP		= svr.Value;
			tc.ServerPort	= sport;
			tc.Port			= port;

			rs	= false;
		}
		if(!rs) tc.Save();

		tc.Show();
	}

	return &tc;
}
