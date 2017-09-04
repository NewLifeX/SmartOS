#include "Kernel\TTime.h"
#include "Kernel\WaitHandle.h"

#include "Net\Socket.h"
#include "Net\NetworkInterface.h"
#include "Net\ITransport.h"

#include "Message\Json.h"
#include "Message\Api.h"

#include "LinkClient.h"

#include "Security\RC4.h"

LinkClient* LinkClient::Current = nullptr;

LinkClient::LinkClient()
	: Routes(String::Compare)
{
	Opened = false;
	Status = 0;

	Cfg = LinkConfig::Current;

	PingTime = 10;
	LoginTime = 0;
	LastSend = 0;
	LastActive = 0;
	Delay = 0;
	MaxNotActive = 0;

	_task = 0;
	ReportStart = -1;
	ReportLength = 0;

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

	CheckReport();
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
	debug_printf("Link <= ");
	msg.Show(true);
#endif // DEBUG

	LastActive = Sys.Ms();

	auto js = msg.Create();

	// 特殊处理响应
	if (msg.Reply) {
		auto code = js["code"].AsInt();
		// 未登录时马上重新登录
		if (code == 401) {
			Status = 0;
			Sys.SetTask(_task, true, 0);
			return;
		}
	}

	auto act = js["action"].AsString();
	if (act == "Device/Login")
		OnLogin(msg);
	else if (act == "Device/Ping")
		OnPing(msg);
	else if (act == "Read")
		OnRead(msg);
	else if (act == "Write")
		OnWrite(msg);
	else if (!msg.Reply) {
		// 调用全局动作
		auto act2 = act;
		String rs;
		int code = Api.Invoke(act2.GetBuffer(), this, js["args"].AsString(), rs);

		Reply(act, msg.Seq, code, rs);
	}

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
	debug_printf("Link => ");
	msg.Show(true);
#endif

	// 发送
	return Send(msg);
}

bool LinkClient::Reply(const String& action, int seq, int code, const Json& result) {
	// 消息缓冲区，跳过头部
	char cs[512];

	// 格式化消息
	auto& msg = *(LinkMessage*)cs;
	msg.Init();
	msg.Reply = true;
	msg.Error = code != 0;
	msg.Seq = seq;

	auto js = msg.Create(sizeof(cs));
	js.Add("action", action);
	js.Add("code", code);
	js.Add("result", result);

	auto str = js.ToString();

	// 长度
	msg.Length = str.Length();
	msg.Code = 1;

#if DEBUG
	debug_printf("Link => ");
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

	json.Add("ip", Master->Host->IP.ToString());

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

		auto key = rs["Key"].AsString();

		auto user = rs["user"].AsString();
		if (user) {
			debug_printf("注册成功！ user=");
			user.Show(true);

			// 保存用户密码
			Cfg->User() = user;
			Cfg->Pass() = rs["pass"].AsString();

			// 保存服务器地址
			auto svr = rs["server"].AsString();
			if (svr) Cfg->Server() = svr;

			Cfg->Show();
			Cfg->Save();

			Status = 0;

			Sys.SetTask(_task, true, 0);
		}
		else {
			debug_printf("登录成功！ Key=");
			key.Show(true);

			// 登录成功后加大心跳间隔
			Sys.SetTaskPeriod(_task, PingTime * 1000);
		}
	}
}

// 心跳，用于保持与对方的活动状态
void LinkClient::Ping()
{
	TS("LinkClient::Ping");

	if (LastActive > 0 && LastActive + 300000 < Sys.Ms())
	{
		// 30秒无法联系，服务端可能已经掉线，重启Hello任务
		debug_printf("300秒无法联系，服务端可能已经掉线，重新登录\r\n");

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
	json.Add("Data", Store.Data.ToHex());

	Invoke("Device/Ping", json);
}

void LinkClient::OnPing(LinkMessage& msg)
{
	TS("LinkClient::OnPing");
	if (!msg.Reply || msg.Error) return;

	auto js = msg.Create();

	int ms = js["Time"].AsInt();
	int cost = (int)(Sys.Ms() - ms);

	if (Delay)
		Delay = (Delay + cost) / 2;
	else
		Delay = cost;

	debug_printf("心跳延迟 %dms / %dms \r\n", cost, Delay);

	// 同步本地时间
	int serverTime = js["ServerSeconds"].AsInt();
	if (serverTime > 1000) ((TTime&)Time).SetTime(serverTime);
}

void LinkClient::Reset(const String& reason)
{
	Json js;
	js.Add("time", DateTime::Now().TotalSeconds());
	js.Add("reason", reason);

	Invoke("Reset", js);

	debug_printf("设备500ms后重置\r\n");

	Sys.Sleep(500);

	Config::Current->RemoveAll();
	Sys.Reboot();
}

void LinkClient::Reboot(const String& reason)
{
	Json js;
	js.Add("time", DateTime::Now().TotalSeconds());
	js.Add("reason", reason);

	Invoke("Reboot", js);
}

void LinkClient::OnRead(LinkMessage& msg)
{
	TS("LinkClient::OnRead");
	if (msg.Reply) return;

	auto js = msg.Create();
	auto args = js["args"];
	int start = args["start"].AsInt();
	int size = args["size"].AsInt();

	ByteArray bs(size);
	int len = Store.Read(start, bs);
	bs.SetLength(len);

	// 响应
	Json rs;
	rs.Add("start", 0);
	rs.Add("size", bs.Length());
	rs.Add("data", bs.ToHex());

	Reply(js["action"].AsString(), msg.Seq, 0, rs);
}

void LinkClient::OnWrite(LinkMessage& msg)
{
	TS("LinkClient::OnWrite");
	if (msg.Reply) return;

	auto js = msg.Create();
	auto args = js["args"];
	int start = args["start"].AsInt();
	auto data = args["data"].AsString().ToHex();

	Store.Write(start, data);
	auto& bs = Store.Data;

	// 响应
	Json rs;
	rs.Add("start", 0);
	rs.Add("size", bs.Length());
	rs.Add("data", bs.ToHex());

	Reply(js["action"].AsString(), msg.Seq, 0, rs);
}

void LinkClient::Write(int start, const Buffer& bs)
{
	Json js;
	js.Add("start", start);
	js.Add("data", bs.ToHex());

	Invoke("Device/Write", js);
}

void LinkClient::Write(int start, byte dat)
{
	Write(start, Buffer(&dat, 1));
}

void LinkClient::ReportAsync(int start, uint length)
{
	if (start<0 || start + (int)length > Store.Data.Length())
	{
		debug_printf("布置异步上报数据失败\r\n");
		debug_printf("start=%d  len:%d  data.len:%d\r\n", start, length, Store.Data.Length());
		return;
	}

	ReportStart = start;
	ReportLength = length;

	// 延迟上报，期间有其它上报任务到来将会覆盖
	Sys.SetTask(_task, true, 20);
}

bool LinkClient::CheckReport()
{
	TS("LinkClient::CheckReport");

	auto offset = ReportStart;
	int len = ReportLength;

	if (offset < 0 || len <= 0) return false;

	// 检查索引，否则数组越界
	auto& bs = Store.Data;
	if (bs.Length() >= offset + len) Write(offset, bs.Sub(offset, len));

	ReportStart = -1;

	return true;
}

// 快速建立客户端，注册默认Api
LinkClient* LinkClient::Create(cstring server, const Buffer& store)
{
	// Flash最后一块作为配置区
	if (Config::Current == nullptr) Config::Current = &Config::CreateFlash();

	// 初始化令牌网
	auto tk = LinkConfig::Create(server);

	auto tc = new LinkClient();
	tc->Cfg = tk;
	tc->MaxNotActive = 8 * 60 * 1000;

	/*// 重启
	tc->Register("Gateway/Restart", &LinkClient::InvokeRestart, tc);
	// 重置
	tc->Register("Gateway/Reset", &LinkClient::InvokeReset, tc);
	// 设置远程地址
	tc->Register("Gateway/SetRemote", &LinkClient::InvokeSetRemote, tc);
	// 获取远程配置信息
	tc->Register("Gateway/GetRemote", &LinkClient::InvokeGetRemote, tc);
	// 获取所有Invoke命令
	tc->Register("Api/All", &LinkClient::InvokeGetAllApi, tc);*/

	if (store.Length()) tc->Store.Data.Set(store.GetBuffer(), store.Length());

	return tc;
}
