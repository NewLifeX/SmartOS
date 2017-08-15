#include "Kernel\TTime.h"
#include "Kernel\WaitHandle.h"

#include "Net\Socket.h"
#include "Net\NetworkInterface.h"

#include "Message\Json.h"

#include "LinkClient.h"

#include "Security\RC4.h"

LinkClient* LinkClient::Current = nullptr;

//static void BroadcastHelloTask(void* param);

LinkClient::LinkClient()
	: Routes(String::Compare)
{
	Opened = false;
	Status = 0;

	LoginTime = 0;
	LastSend = 0;
	LastActive = 0;
	Delay = 0;
	MaxNotActive = 0;

	NextReport = -1;
	ReportLength = 0;

	_Expect = nullptr;

	assert(!Current, "只能有一个令牌客户端实例");
	Current = this;
}

void LinkClient::Open()
{
	if (Opened) return;

	TS("LinkClient::Open");

	// 令牌客户端定时任务
	_task = Sys.AddTask(&LinkClient::LoopTask, this, 0, 1000, "令牌客户");

	// 启动时记为最后一次活跃接收
	LastActive = Sys.Ms();

	Opened = true;
}

void LinkClient::Close()
{
	if (!Opened) return;

	Sys.RemoveTask(_task);
	//Sys.RemoveTask(_taskBroadcast);

	Opened = false;
}

bool LinkClient::Send(LinkMessage& msg) {
	return false;
}

bool LinkClient::Invoke(String& action, String& args) {
	// 消息缓冲区，跳过头部
	char cs[512];
	String str(&cs[sizeof(LinkMessage)], sizeof(cs) - sizeof(LinkMessage));

	// 构造内容
	str += "{\"action\":\"";
	str += action;
	str += "\",\"args\":}";
	str += args;
	str += "\"}";

	// 格式化消息
	auto& msg = *(LinkMessage*)cs;
	msg.Init();

	// 长度
	msg.Length = str.Length();
	msg.Code = 1;

	// 序列号
	static byte _g_seq = 1;
	msg.Seq = _g_seq++;
	if (_g_seq == 0)_g_seq++;

	// 发送
	return Send(msg);
}
