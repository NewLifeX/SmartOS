#include "Net\IPEndPoint.h"

#include "LinkConfig.h"

#include "Message\Json.h"
#include "Message\Api.h"

LinkConfig* LinkConfig::Current = nullptr;

LinkConfig::LinkConfig() : ConfigBase()
{
	_Name = "LinkCfg";
	_Start = &Length;
	_End = &TagEnd;
	Init();
}

void LinkConfig::Init()
{
	ConfigBase::Init();

	Length = Size();
}

void LinkConfig::Show() const
{
#if DEBUG
	debug_printf("LinkConfig::物联配置：\r\n");

	debug_printf("\t服务: %s \r\n", _Server);
	debug_printf("\t登录: %s \r\n", _User);
	debug_printf("\t密码: %s \r\n", _Pass);
#endif
}

LinkConfig* LinkConfig::Create(cstring server)
{
	// Flash最后一块作为配置区
	if (Config::Current == nullptr) Config::Current = &Config::CreateFlash();

	static LinkConfig tc;
	if (!Current)
	{
		LinkConfig::Current = &tc;
		tc.Init();
		tc.Load();

		auto uri = tc.Uri();

		// 默认 UDP 不允许 unknown
		if (uri.Type == NetType::Unknown) uri.Type = NetType::Udp;

		// 如果服务器配置为空，则使用外部地址
		auto svr = tc.Server();
		if (!svr)
		{
			svr = server;

			tc.Save();
		}

		tc.Show();

		// 注册全局动作
		Api.Register("SetServer", &LinkConfig::SetServer, &tc);
		Api.Register("GetServer", &LinkConfig::GetServer, &tc);
	}

	return &tc;
}

// 设置服务器地址
int LinkConfig::SetServer(const String& args, String& result)
{
	if (!args) return -1;

	Json js(args);

	auto svr = js["server"].AsString();
	if (!svr) return -1;

	Server() = svr;
	Save();
	Show();

	// 3秒后重启
	Sys.Reboot(3000);

	return 0;
}

// 获取服务器地址
int LinkConfig::GetServer(const String& args, String& result)
{
	Json js(result);
	js.Add("server", Server());

	return 0;
}
