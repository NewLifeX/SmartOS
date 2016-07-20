#include "Gateway.h"
#include "Config.h"

#include "Security\MD5.h"
#include "Security\Crc.h"
#include "..\Message\BinaryPair.h"

// 循环间隔
#define LOOP_Interval	10000

bool TokenToTiny(const TokenMessage& msg, TinyMessage& msg2);
void TinyToToken(const TinyMessage& msg, TokenMessage& msg2);
Gateway* Gateway::Current = nullptr;

// 本地网和远程网一起实例化网关服务
Gateway::Gateway()
{
	Server = nullptr;
	Client = nullptr;
	Led = nullptr;
	pDevMgmt = nullptr;

	Running = false;
}

Gateway::~Gateway()
{
	Stop();

	delete Server;
	Server = nullptr;

	delete Client;
	Client = nullptr;
}

// 启动网关。挂载本地和远程的消息事件
void Gateway::Start()
{
	if (Running) return;

	TS("Gateway::Start");

	assert(Server, "微网服务端未设置");
	assert(Client, "令牌客户端未设置");

	Server->Received = [](void* s, Message& msg, void* p) { return ((Gateway*)p)->OnLocal((TinyMessage&)msg); };
	Server->Param = this;

	Client->Received = [](void* s, Message& msg, void* p) { return ((Gateway*)p)->OnRemote((TokenMessage&)msg); };
	Client->Param = this;

	Client->Register("Gateway/RestStart", InvokeRestStart, this);
	Client->Register("Gateway/RestBoot", InvokeRestBoot, this);

	debug_printf("\r\nGateway::Start \r\n");

	Server->Start();

	// 如果全局都没有设备列表，那就直接创建一个
	if (DevicesManagement::Current)
		pDevMgmt = DevicesManagement::Current;
	else
		pDevMgmt = new DevicesManagement();

	// 设备列表服务于Token  保存token的“联系方式”
	//pDevMgmt->Port = Client;
	pDevMgmt->SetTokenClient(Client);

	if (pDevMgmt->Length() == 0)
	{
		// 如果全局设备列表为空，则添加一条Addr=1的设备
		auto dv = new Device();
		dv->Address = 1;
		dv->Kind = Sys.Code;
		dv->LastTime = Sys.Seconds();

		dv->HardID = Sys.ID;
		dv->Name = Sys.Name;

		//pDevMgmt->PushDev(dv);
		// 标记为永久在线设备
		dv->Flag.BitFlag.OnlineAlws = 1;
		pDevMgmt->DeviceRequest(DeviceAtions::Register, dv);
	}

	Client->Open();
	_task = Sys.AddTask(Loop, this, 10000, LOOP_Interval, "设备任务");

	Running = true;
}

// 停止网关。取消本地和远程的消息挂载
void Gateway::Stop()
{
	if (!Running) return;

	Sys.RemoveTask(_task);

	Server->Received = nullptr;
	Server->Param = nullptr;
	Client->Received = nullptr;
	Client->Param = nullptr;

	Running = false;
}

// 数据接收中心 TinyServer 上级
bool Gateway::OnLocal(const TinyMessage& msg)
{
	TS("Gateway::OnLocal");

	// 应用级消息转发
	if (msg.Code >= 0x10 && msg.Dest == Server->Cfg->Address)
	{
		TokenMessage tmsg;

		TinyToToken(msg, tmsg);

		Client->Send(tmsg);
	}

	return true;
}

// 数据接收中心 TokenClient 上级
bool Gateway::OnRemote(const TokenMessage& msg)
{
	TS("Gateway::OnRemote");

	if (msg.Code == 0x20)
	{
		OnMode(msg);
	}

	if (msg.Code == 0x02)
	{
		if (msg.Code == 0x02 && msg.Reply && Client->Token != 0)
		{
			// 登录以后自动发送设备列表和设备信息
			// 遍历发送所有设备信息
			pDevMgmt->SendDevicesIDs();
			return true;
		}
	}

	// 应用级消息转发
	if (msg.Code >= 0x10 && !msg.Error && msg.Length <= Server->Control->Port->MaxSize - TinyMessage::MinSize)
	{
		//debug_printf("Gateway::Remote ");
		//msg.Show();

		TinyMessage tmsg;
		if (!TokenToTiny(msg, tmsg)) return true;

		bool rs = Server->Dispatch(tmsg);
		if (!rs) return false;

		TokenMessage msg2;
		TinyToToken(tmsg, msg2);

		//msg2.Show();
		return Client->Reply(msg2);
	}

	return true;
}

// 学习模式 0x20
void Gateway::SetMode(uint time)
{
	TS("Gateway::SetMode");

	Server->Study = time > 0;

	// 定时退出学习模式
	_Study = time;

	// 设定小灯快闪时间，单位毫秒
	if (Led) Led->Write(time ? time * 1000 : 100);

	if (time)
		debug_printf("进入 学习模式 %d 秒\r\n", time);
	else
		debug_printf("退出 学习模式\r\n");

	TokenMessage msg;
	msg.Code = 0x20;
	msg.Length = 1;
	msg.Data[0] = time;

	Client->Reply(msg);
}

// 获取学习模式 返回sStudy
uint Gateway::GetMode()
{
	return _Study;
}

// 清空
void Gateway::Clear()
{
	TS("Gateway::Clear()");

	TokenMessage msg;
	msg.Code = 0x35;
	msg.Length = 1;
	msg.Data[0] = 0;
	Client->Reply(msg);
}

bool Gateway::OnMode(const Message& msg)
{
	msg.Show();

	TS("Gateway::OnMode");

	int time = 30;
	if (msg.Length > 0) time = msg.Data[0];

	SetMode(time);

	return true;
}

bool TokenToTiny(const TokenMessage& msg, TinyMessage& tny)
{
	if (msg.Length == 0) return false;

	TS("TokenToTiny");

	tny.Code = msg.Code;
	// 处理Reply标记
	tny.Reply = msg.Reply;
	tny.Error = msg.Error;
	// 第一个字节是节点设备地址
	// tny.Dest	= msg.Data[0];
	//if(msg.Length > 1) Buffer(tny.Data, msg.Length - 1)	= &msg.Data[1];
	//tny.Length	= msg.Length - 1;

	TokenDataMessage dm;
	dm.ReadMessage(msg);
	tny.Dest = dm.ID;

	auto ms = tny.ToStream();
	ms.WriteEncodeInt(dm.Start);
	// 不管什么指令 有就写  没就不写
	if (dm.Size)ms.WriteEncodeInt(dm.Size);
	if (dm.Data.Length() != 0)ms.Write(dm.Data);

	tny.Length = ms.Position();
	return true;
}

void TinyToToken(const TinyMessage& msg, TokenMessage& msg2)
{
	TS("TinyToToken");

	// 处理Reply标记
	msg2.Code = msg.Code;
	msg2.Reply = msg.Reply;
	msg2.Error = msg.Error;

	auto ms = msg.ToStream();

	TokenDataMessage dm;
	//dm.ReadMessage(msg2);	// 很显然 什么都读不到
	dm.ID = ms.ReadByte();
	dm.Start = ms.ReadEncodeInt();

	// 存在三种情况  云读设备的回复   云写设备的回复  设备上报
	// 云读设备的回复  设备上报  没有长度
	// 云写设备的回复 不确定
	auto isRead = (msg.Code == 0x05 || msg.Code == 0x15);
	if ((!isRead) && msg.Reply)
	{
		dm.Size = ms.ReadEncodeInt();
	}

	dm.Data = ByteArray(ms.GetBuffer() + ms.Position(), ms.Remain());
	dm.WriteMessage(msg2);
}

Gateway* Gateway::CreateGateway(TokenClient* client, TinyServer* server)
{
	debug_printf("\r\nGateway::CreateGateway \r\n");

	Gateway* gw = Current;
	if (gw)
	{
		if ((client == nullptr || gw->Client == client) &&
			(server == nullptr || gw->Server == server)) return gw;

		delete gw;
	}

	gw = new Gateway();
	gw->Client = client;
	gw->Server = server;

	Current = gw;

	return gw;
}
// 设备上线下线报备
void Gateway::Loop(void* param)
{
	TS("Gateway::Loop");

	auto gw = (Gateway*)param;

	// 检测自动退出学习模式
	if (gw->_Study)
	{
		gw->_Study -= LOOP_Interval / 1000;
		if (gw->_Study <= 0)
		{
			gw->_Study = 0;

			gw->SetMode(0);
		}
	}
	gw->pDevMgmt->MaintainState();
}
/******************************** invoke 调用********************************/

bool Gateway::InvokeRestStart(void * param, const BinaryPair& args, Stream& result)
{
	BinaryPair res(result);
	res.Set("RestStar", (byte)01);

	debug_printf("500ms后重启\r\n");
	Sys.Sleep(500);
	Sys.Reset();

	return true;
}
bool Gateway::InvokeRestBoot(void * param, const BinaryPair& args, Stream& result)
{
	BinaryPair res(result);
	res.Set("RestBoot", (byte)01);
	Config::Current->RemoveAll();

	debug_printf("500ms后重置\r\n");
	Sys.Sleep(500);
	Sys.Reset();

	return true;
}
