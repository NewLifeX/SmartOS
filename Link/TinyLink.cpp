#include "Kernel\TTime.h"
#include "Kernel\WaitHandle.h"

#include "Net\ITransport.h"

#include "Message\Json.h"
#include "Message\Api.h"

#include "TinyLink.h"

TinyLink::TinyLink()
{
	Opened = false;

	PingTime = 10;

	_task = 0;
}

void TinyLink::Open()
{
	if (Opened) return;

	TS("TinyLink::Open");
	assert(Port, "未指定Port");

	Port->Open();
	_task = Sys.AddTask(&TinyLink::LoopTask, this, 0, 1000, "微联定时");

	Opened = true;
}

void TinyLink::Close()
{
	if (!Opened) return;

	Port->Close();
	Sys.RemoveTask(_task);

	Opened = false;
}

// 定时任务
void TinyLink::LoopTask()
{
	TS("TinyLink::LoopTask");

	if (!Logined)
		Login();
	else
		Ping();
}

uint TinyLink::Dispatch(ITransport* port, Buffer& bs, void* param, void* param2)
{
	TS("TinyLink::Dispatch");

	byte* buf = bs.GetBuffer();
	uint len = bs.Length();

	auto& client = *(TinyLink*)param;

	auto& msg = *(LinkMessage*)buf;
	if (len - sizeof(msg) < msg.Length) {
		debug_printf("TinyLink::Receive %d < %d 数据包长度不足\r\n", len - sizeof(msg), msg.Length);
		return 0;
	}

	client.OnReceive(msg);

	return 0;
}

// 接收处理
void TinyLink::OnReceive(LinkMessage& msg)
{
	TS("TinyLink::OnReceive");

#if DEBUG
	debug_printf("Link <= ");
	msg.Show(true);
#endif // DEBUG

	auto js = msg.Create();

	if (!msg.Reply) {
		// 调用全局动作
		auto act = js["action"].AsString();
		auto act2 = act;
		String rs;
		int code = Api.Invoke(act2.GetBuffer(), js["args"].AsString(), rs);

		Reply(act, msg.Seq, code, rs);
	}
}

bool TinyLink::Send(LinkMessage& msg, const String& data) {
	auto& str = data;

	// 长度
	msg.Length = str.Length();
	msg.Code = 1;

#if DEBUG
	debug_printf("Link => ");
	str.Show(true);
#endif

	// 发送
	Port->Write(Buffer(&msg, sizeof(msg)));
	return Port->Write(str);
}

bool TinyLink::Invoke(const String& action, const Json& args) {
	LinkMessage msg;
	msg.Init();

	Json js;
	js.Add("action", action);
	js.Add("args", args);

	// 序列号
	static byte _g_seq = 1;
	msg.Seq = _g_seq++;
	if (_g_seq == 0)_g_seq++;

	return Send(msg, js.ToString());
}

bool TinyLink::Reply(const String& action, int seq, int code, const String& result) {
	LinkMessage msg;
	msg.Init();
	msg.Reply = true;
	msg.Error = code != 0;
	msg.Seq = seq;

	Json js;
	js.Add("action", action);
	js.Add("code", code);
	js.Add("result", result);

	return Send(msg, js.ToString());
}

// 登录
void TinyLink::Login()
{
	TS("TinyLink::Login");

	Json json;

	json.Add("user", Buffer(Sys.ID, 16).ToHex());

	ushort code = _REV16(Sys.Code);
	json.Add("type", Buffer(&code, 2).ToHex());
	json.Add("agent", Sys.Name);
	json.Add("version", Version(Sys.Ver).ToString());

	Invoke("Login", json);
}

void TinyLink::OnLogin(LinkMessage& msg)
{
	TS("TinyLink::OnLogin");
	if (!msg.Reply) return;

	auto js = msg.Create();
	auto rs = js["result"];
	if (msg.Error)
	{
		Logined = false;
#if DEBUG
		debug_printf("登录失败！");
		rs.AsString().Show(true);
#endif
	}
	else
	{
		Logined = true;

		debug_printf("登录成功！\r\n");

		// 登录成功后加大心跳间隔
		Sys.SetTaskPeriod(_task, PingTime * 1000);
	}
}

// 心跳，用于保持与对方的活动状态
void TinyLink::Ping()
{
	TS("TinyLink::Ping");

	Json json;

	auto ms = (int)Sys.Ms();
	json.Add("Time", ms);
	json.Add("Data", Store.Data.ToHex());

	Invoke("Device/Ping", json);
}

void TinyLink::OnPing(LinkMessage& msg)
{
	TS("TinyLink::OnPing");
	if (!msg.Reply || msg.Error) return;

	auto js = msg.Create();

	int ms = js["Time"].AsInt();
	int cost = (int)(Sys.Ms() - ms);

	debug_printf("心跳延迟 %dms \r\n", cost);

	// 同步本地时间
	int serverTime = js["ServerSeconds"].AsInt();
	if (serverTime > 1000) ((TTime&)Time).SetTime(serverTime);
}

// 快速建立客户端，注册默认Api
TinyLink* TinyLink::Create(ITransport& port, const Buffer& store)
{
	auto tk = new TinyLink();
	tk->Port = &port;

	if (store.Length()) tk->Store.Data.Set(store.GetBuffer(), store.Length());

	return tk;
}
