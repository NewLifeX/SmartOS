﻿#include "Kernel\TTime.h"
#include "Kernel\WaitHandle.h"

#include "Net\Socket.h"
#include "Net\NetworkInterface.h"
#include "Net\ITransport.h"

#include "Message\Json.h"

#include "LinkClient.h"

#include "Security\RC4.h"

LinkClient* LinkClient::Current = nullptr;

LinkClient::LinkClient()
	: Routes(String::Compare)
{
	Opened = false;
	Status = 0;

	Cfg = LinkConfig::Current;

	LoginTime = 0;
	LastSend = 0;
	LastActive = 0;
	Delay = 0;
	MaxNotActive = 0;

	assert(!Current, "只能有一个物联客户端实例");
	Current = this;
}

void LinkClient::Open()
{
	if (Opened) return;

	TS("LinkClient::Open");
	assert(Cfg, "未指定物联配置Cfg");

	// 令牌客户端定时任务
	_task = Sys.AddTask(&LinkClient::LoopTask, this, 0, 1000, "物联客户");

	// 启动时记为最后一次活跃接收
	LastActive = Sys.Ms();

	Opened = true;
}

void LinkClient::Close()
{
	if (!Opened) return;

	Sys.RemoveTask(_task);

	Opened = false;
}

void LinkClient::CheckNet()
{
	auto mst = Master;

	// 检测主链接
	if (!mst)
	{
		auto uri = Cfg->Uri();
		// 创建连接服务器的Socket
		auto socket = Socket::CreateRemote(uri);
		if (!socket) return;

		// 注册收到数据事件
		auto port = dynamic_cast<ITransport*>(socket);
		port->Register(Dispatch, this);

		Master = socket;

		debug_printf("LinkClient::CheckNet %s 成功创建主连接\r\n", socket->Host->Name);

		// 已连接时，减慢网络检查速度
		Sys.SetTaskPeriod(_task, 5000);
	}
	// 检测主链接是否已经断开
	else if (!mst->Host->Linked)
	{
		debug_printf("LinkClient::CheckNet %s断开，切换主连接\r\n", mst->Host->Name);

		delete mst;
		Master = nullptr;

		Status = 0;

		// 未连接时，加快网络检查速度
		Sys.SetTaskPeriod(_task, 1000);
	}
}

// 定时任务
void LinkClient::LoopTask()
{
	TS("LinkClient::LoopTask");

	// 最大不活跃时间ms，超过该时间时重启系统
	// WiFi触摸开关建议5~10分钟，网关建议5分钟
	// MaxNotActive 为零便不考虑重启
	if (MaxNotActive != 0 && LastActive + MaxNotActive < Sys.Ms()) Sys.Reboot();

	CheckNet();
	if (!Master) return;

	// 状态。0准备、1握手完成、2登录后
	switch (Status)
	{
	case 0:
		Login();

		// 登录成功后，心跳一次，把数据同步上去
		Sys.Sleep(1000);
		if (Status >= 2) Ping();
		break;

	case 2:
		Ping();
		break;
	}
}

uint LinkClient::Dispatch(ITransport* port, Buffer& bs, void* param, void* param2)
{
	TS("LinkClient::Dispatch");

	byte* buf = bs.GetBuffer();
	uint len = bs.Length();

	auto& client = *(LinkClient*)param;

	auto& msg = *(LinkMessage*)buf;
	if (len - sizeof(msg) < msg.Length) {
		debug_printf("LinkClient::Receive %d < %d 数据包长度不足\r\n", len - sizeof(msg), msg.Length);
		return 0;
	}

	client.OnReceive(msg);

	return 0;
}

// 接收处理
void LinkClient::OnReceive(LinkMessage& msg)
{
	TS("LinkClient::OnReceive");

#if DEBUG
	debug_printf("LinkClient::Receive <= ");
	msg.Show(true);
#endif // DEBUG

	auto js = msg.Create();

	auto act = js["action"].AsString();

	if (act == "login")
		OnLogin(msg);
	else if (act == "ping")
		OnLogin(msg);

	// 外部公共消息事件
	//Received(msg, *this);
}

bool LinkClient::Send(const LinkMessage& msg) {

	return Master->Send(msg.GetBuffer());
}

bool LinkClient::Invoke(const String& action, const Json& args) {
	// 消息缓冲区，跳过头部
	char cs[512];

	// 格式化消息
	auto& msg = *(LinkMessage*)cs;
	msg.Init();

	auto js = msg.Create(sizeof(cs));
	js.Add("action", action);
	js.Add("args", args);

	auto str = js.ToString();

	// 长度
	msg.Length = str.Length();
	msg.Code = 1;

	// 序列号
	static byte _g_seq = 1;
	msg.Seq = _g_seq++;
	if (_g_seq == 0)_g_seq++;

#if DEBUG
	debug_printf("LinkClient::Send => ");
	msg.Show(true);
#endif

	// 发送
	return Send(msg);
}

// 登录
void LinkClient::Login()
{
	TS("LinkClient::Login");

	Json json;

	// 已有用户名则直接登录，否则用机器码注册
	auto user = Cfg->User();
	if (user) {
		json.Add("user", user);

		// 原始密码对盐值进行加密，得到登录密码
		auto now = DateTime::Now().TotalMs();
		auto arr = Buffer(&now, 8);
		ByteArray bs;
		bs = arr;
		RC4::Encrypt(arr, Cfg->Pass());
		// 散列明文和密码连接在一起
		auto pass = bs.ToHex();
		pass += arr.ToHex();

		json.Add("pass", pass);
	}
	else {
		json.Add("user", Buffer(Sys.ID, 16).ToHex());
	}

	ushort code = _REV16(Sys.Code);
	json.Add("type", Buffer(&code, 2).ToHex());
	json.Add("agent", Sys.Name);
	json.Add("version", Version(Sys.Ver).ToString());

	Invoke("Device/Login", json);
}

void LinkClient::OnLogin(LinkMessage& msg)
{
	TS("LinkClient::OnLogin");
	if (!msg.Reply) return;

	auto js = msg.Create();
	auto rs = js["result"];
	if (msg.Error)
	{
		Status = 0;
#if DEBUG
		debug_printf("登录失败！");
		rs.AsString().Show(true);
#endif
	}
	else
	{
		Status = 2;

		auto key = rs["key"].AsString();
		debug_printf("登录成功！ Key=");
		key.Show(true);

		// 登录成功后加大心跳间隔
		Sys.SetTaskPeriod(_task, 60000);
	}
}

// 心跳，用于保持与对方的活动状态
void LinkClient::Ping()
{
	TS("LinkClient::Ping");

	if (LastActive > 0 && LastActive + 300000 < Sys.Ms())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Hello任务
		debug_printf("300秒无法联系，服务端可能已经掉线，重新开始握手\r\n");

		delete Master;
		Master = nullptr;

		Status = 0;

		Sys.SetTaskPeriod(_task, 5000);

		return;
	}

	// 30秒内发过数据，不再发送心跳
	if (LastSend > 0 && LastSend + 60000 > Sys.Ms()) return;

	Json json;

	//json.Add("Data", Store.Data.ToHex());

	// 原始密码对盐值进行加密，得到登录密码
	auto ms = (int)Sys.Ms();
	json.Add("Time", ms);

	Invoke("Device/Ping", json);
}

void LinkClient::OnPing(LinkMessage& msg)
{
	TS("LinkClient::OnPing");
	if (!msg.Reply) return;

	msg.Show(true);
	if (msg.Error) return;

	auto js = msg.Create();

	int ms = js["Time"].AsInt();
	int cost = (int)(Sys.Ms() - ms);

	if (Delay)
		Delay = (Delay + cost) / 2;
	else
		Delay = cost;

	debug_printf("心跳延迟 %dms / %dms \r\n", cost, Delay);

	// 同步本地时间
	int serverTime = js["ServerTime"].AsInt();
	if (serverTime > 1000) ((TTime&)Time).SetTime(serverTime);
}
