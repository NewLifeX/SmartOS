#include "MiniGateway.h"
#include "Config.h"

#include "Security\MD5.h"
#include "Security\Crc.h"

// 循环间隔
#define LOOP_Interval	10000

bool TokenToTiny(const TokenMessage& msg, TinyMessage& msg2);
void TinyToToken(const TinyMessage& msg, TokenMessage& msg2);
MiniGateway* MiniGateway::Current	= nullptr;

// 本地网和远程网一起实例化网关服务
MiniGateway::MiniGateway()
{
	TkClient	= nullptr;
	Led			= nullptr;

	Running		= false;
}

MiniGateway::~MiniGateway()
{
	Stop();

	delete TkClient;
	TkClient = nullptr;
}

// 启动网关。挂载本地和远程的消息事件
void MiniGateway::Start()
{
	if(Running) return;

	TS("MiniGateway::Start");

	assert(TkClient, "令牌客户端未设置");

	TkClient->Received	= [](void* s, Message& msg, void* p){ return ((MiniGateway*)p)->OnRemote((TokenMessage&)msg); };
	TkClient->Param		= this;

	debug_printf("\r\nMiniGateway::Start \r\n");

	{
		_Dev.Address	= 0x01;
		_Dev.Kind		= Sys.Code;
		_Dev.LastTime	= Sys.Seconds();
		_Dev.Logined = true;
		
		_Dev.HardID	= Sys.ID;
		_Dev.Name	= Sys.Name;
	}
	
	TkClient->Open();
	_task	= Sys.AddTask(Loop, this, 10000, LOOP_Interval, "设备任务");

	Running = true;
}

// 停止网关。取消本地和远程的消息挂载
void MiniGateway::Stop()
{
	if(!Running) return;

	Sys.RemoveTask(_task);

	// Server->Received	= nullptr;
	// Server->Param		= nullptr;
	TkClient->Received	= nullptr;
	TkClient->Param		= nullptr;

	Running = false;
}

/*
// 数据接收中心
bool MiniGateway::OnLocal(const TinyMessage& msg)
{
	TS("MiniGateway::OnLocal");

//	auto dv = Server->Current;
//	if(dv)
//	{
//		switch(msg.Code)
//		{
//			case 0x01:
//				DeviceRequest(DeviceAtions::Register, dv);
//				DeviceRequest(DeviceAtions::Online, dv);
//				break;
//			case 0x02:
//				DeviceRequest(DeviceAtions::Delete, dv);
//				break;
//		}
//	}
//
//	// 应用级消息转发
//	if(msg.Code >= 0x10 && msg.Dest == Server->Cfg->Address)
	{
		TokenMessage tmsg;

		TinyToToken(msg, tmsg);

		TkClient->Send(tmsg);
	}

	return true;
}
*/

bool MiniGateway::OnRemote(const TokenMessage& msg)
{
	TS("MiniGateway::OnRemote");

	switch(msg.Code)
	{
		case 0x02:
			// 登录以后自动发送设备列表和设备信息
			if(msg.Reply && TkClient->Token != 0)
			{
				// 遍历发送所有设备信息
				SendDevices(DeviceAtions::List, nullptr);
			}
			break;

		case 0x20:
			return OnMode(msg);
		case 0x21:
			return DeviceProcess(msg);
	}

	// 应用级消息转发
	if(msg.Code >= 0x10 && !msg.Error /* && msg.Length <= Server->Control->Port->MaxSize - TinyMessage::MinSize*/)
	{
		//msg.Show();

		TinyMessage tmsg;
		if(!TokenToTiny(msg, tmsg)) return true;

		// 处理消息
		bool rs = Dispatch(tmsg);
		// if(!rs) return false;
        // 
		// TokenMessage msg2;
		// TinyToToken(tmsg, msg2);
        // 
		// return TkClient->Reply(msg2);
	}

	return true;
}

// 设备列表 0x21
bool MiniGateway::SendDevices(DeviceAtions act, const Device* dv)
{
	TS("MiniGateway::SendDevices");

	if(TkClient->Status < 2) return false;
	if(!(dv == nullptr || dv->Address == _Dev.Address))return false;

	TokenMessage msg;
	msg.Code = 0x21;
	
	int count = 1;
	
	byte buf[512];
	MemoryStream ms(buf, ArrayLength(buf));
	ms.Write((byte)act);
	ms.Write((byte)count);
	
	_Dev.WriteMessage(ms);
	
	msg.Length 	= ms.Position();
	msg.Data 	= ms.GetBuffer();

#if DEBUG
	switch(act)
	{
		case DeviceAtions::List:
			debug_printf("发送设备列表 共%d个\r\n", count);
			break;
		case DeviceAtions::Online:
			debug_printf("节点上线 ID=0x%02X \t", dv->Address);
			break;
		case DeviceAtions::Offline:
			debug_printf("节点下线 ID=0x%02X \t", dv->Address);
			break;
		default: break;
	}
#endif

	if(act == DeviceAtions::List)
		return TkClient->Reply(msg);
	else
		return TkClient->Send(msg);
}

void MiniGateway::SendDevicesIDs()
{
	TokenMessage msg;
	msg.Code	= 0x21;
	auto act	= DeviceAtions::ListIDs;

	MemoryStream ms;
	ms.Write((byte)act);
	// byte len = Server->Devices.Length();
	// byte len = 1;
	ms.Write(_Dev.Address);

	msg.Length 	= ms.Position();
	msg.Data 	= ms.GetBuffer();

	TkClient->Send(msg);
}

// 学习模式 0x20
void MiniGateway::SetMode(uint time)
{
	TS("MiniGateway::SetMode");

	if(Led) Led->Write(time ? time * 1000 : 100);

	if(time)
		debug_printf("进入 学习模式 %d 秒\r\n", time);
	else
		debug_printf("退出 学习模式\r\n");
	debug_printf("无学习模式！！！\r\n");

	TokenMessage msg;
	msg.Code	= 0x20;
	msg.Length	= 1;
	msg.Data[0]	= time;

	TkClient->Reply(msg);
}

// 获取学习模式 返回sStudy
uint MiniGateway::GetMode()
{
	// return _Study;
	return 0x00;
}

// 清空
void MiniGateway::Clear()
{
	TS("MiniGateway::Clear()");

	TokenMessage msg;
	msg.Code	= 0x35;
	msg.Length	= 1;
	msg.Data[0]	= 0;
	TkClient->Reply(msg);
}

bool MiniGateway::OnMode(const Message& msg)
{
	msg.Show();

	TS("MiniGateway::OnMode");

	return true;
}

// 节点消息处理 0x21
void MiniGateway::DeviceRequest(DeviceAtions act, const Device* dv)
{
	TS("MiniGateway::DeviceRequest");

	if(TkClient->Status < 2) return;
	if(dv == nullptr)return;
	
	byte id	= dv->Address;
	switch(act)
	{
		case DeviceAtions::List:
			SendDevices(act, dv);
			return;
		 case DeviceAtions::Online:
		 	debug_printf("节点上线 ID=0x%02X\r\n", id);
		 	break;
		default:
			debug_printf("无法识别的节点操作 Act=%d ID=0x%02X\r\n", (byte)act, id);
			break;
	}

	TokenMessage rs;
	rs.Code	= 0x21;
	rs.Length	= 2;
	rs.Data[0]	= (byte)act;
	rs.Data[1]	= id;

	TkClient->Send(rs);
}

bool MiniGateway::DeviceProcess(const Message& msg)
{
	// 仅处理来自云端的请求
	if(msg.Reply) return false;

	TS("MiniGateway::DeviceProcess");

	auto act	= (DeviceAtions)msg.Data[0];
	byte id		= msg.Data[1];

	TokenMessage rs;
	rs.Code	= 0x21;
	rs.Length	= 2;
	rs.Data[0]	= (byte)act;
	rs.Data[1]	= id;

	switch(act)
	{
		case DeviceAtions::List:
		{
			SendDevices(act, nullptr);
			return true;
		}
		case DeviceAtions::Online:
			break;
		default:
			debug_printf("无法识别的节点操作 Act=%d ID=0x%02X\r\n", (byte)act, id);
			break;
	}

	return true;
}

bool TokenToTiny(const TokenMessage& msg, TinyMessage& tny)
{
	if(msg.Length == 0) return false;

	TS("TokenToTiny");

	tny.Code	= msg.Code;
	// 处理Reply标记
	tny.Reply	= msg.Reply;
	tny.Error	= msg.Error;

	// 第一个字节是节点设备地址
	tny.Dest	= msg.Data[0];
	if(msg.Length > 1) Buffer(tny.Data, msg.Length - 1)	= &msg.Data[1];
	tny.Length	= msg.Length - 1;
	
	return true;
}

void TinyToToken(const TinyMessage& msg, TokenMessage& msg2)
{
	TS("TinyToToken");

	// 处理Reply标记
	msg2.Code = msg.Code;
	msg2.Reply = msg.Reply;
	msg2.Error = msg.Error;
	// 第一个字节是节点设备地址
	msg2.Data[0] = ((TinyMessage&)msg).Src;
	// msg2.Data[0] = _Dev.Address;

	if(msg.Length > 0) Buffer(&msg2.Data[1], msg.Length)	= msg.Data;

	msg2.Length = 1 + msg.Length;
}

MiniGateway* MiniGateway::CreateMiniGateway(TokenClient* TkClient)
{
	debug_printf("\r\nMiniGateway::CreateMiniGateway \r\n");

	MiniGateway* gw	= Current;
	if(gw)
	{
		if(TkClient == nullptr || gw->TkClient == TkClient) return gw;
		delete gw;
	}

	gw = new MiniGateway();
	gw->TkClient	= TkClient;

	Current		= gw;

	return gw;
}

// 设备上线下线报备
void MiniGateway::Loop(void* param)
{
	TS("MiniGateway::Loop");

	auto gw		= (MiniGateway*)param;
	// 没有学习模式
	
	// 未登录不执行任何逻辑
	if(gw->TkClient->Token == 0) return;

	gw->SendDevicesIDs();

	auto now	= Sys.Seconds();
	
	// 不存在下线情况
	auto dv = &(gw->_Dev);
	dv->LastTime = now;
	dv->Logined = true;
	gw->DeviceRequest(DeviceAtions::Online, dv);
	// 定时上报所有数据
	gw->Report();
}


/************业务逻辑***************/

bool MiniGateway::Dispatch(TinyMessage& msg)
{
	if(msg.Reply || msg.Error) return false;

	TS("MiniGateway::Dispatch");

	// bool fw	= true;		// 是否转发给本地

	// 缓存内存操作指令
	switch(msg.Code)
	{
		case 5:
		case 0x15:
		{
			auto now	= Sys.Seconds();
			StoreRead(msg);
			break;
		}
		case 6:
		case 0x16:
			StoreRead(msg);
			break;
	}
	return true;
}

void MiniGateway::StoreRead(const TinyMessage& msg)
{
	if(msg.Length < 2) return;

	auto rs	= msg.CreateReply();
	auto ms	= rs.ToStream();

	DataMessage dm(msg, &ms);

	bool rt	= true;
	if(dm.Offset < 64)
		rt	= dm.ReadData(Store);

	rs.Error	= !rt;
	rs.Length	= ms.Position();

	Reply(rs);
}


void MiniGateway::StoreWrite(const TinyMessage& msg)
{
	
	if(msg.Length < 2) return;

	auto rs	= msg.CreateReply();
	auto ms	= rs.ToStream();

	DataMessage dm(msg, &ms);

	bool rt	= true;
	if(dm.Offset < 64)
	{
		rt	= dm.WriteData(Store, true);
	}

	rs.Error	= !rt;
	rs.Length	= ms.Position();

	Reply(rs);
}

bool MiniGateway::Reply(TinyMessage& msg)
{
	TokenMessage tmsg;
	TinyToToken(msg,tmsg);
	return TkClient->Send(tmsg);
}

bool  MiniGateway::Report()
{
	TinyMessage msg;
	msg.Code = 0x06;
	
	auto ms = msg.ToStream();
	auto & bs = Store.Data;
	
	ms.WriteEncodeInt(0);
	ms.Write(bs);
	msg.Length = ms.Position();
	
	TokenMessage tmsg;
	TinyToToken(msg,tmsg);
	return TkClient->Send(tmsg);
}

bool MiniGateway::Report(uint offset, byte dat)
{
	TinyMessage msg;
	msg.Code	= 0x06;

	auto ms = msg.ToStream();
	ms.WriteEncodeInt(offset);
	ms.Write(dat);
	msg.Length	= ms.Position();

	TokenMessage tmsg;
	TinyToToken(msg,tmsg);
	return TkClient->Send(tmsg);
	//return Send(msg);
}

bool MiniGateway::Report(uint offset, const Buffer& bs)
{
	TinyMessage msg;
	msg.Code	= 0x06;

	auto ms = msg.ToStream();
	ms.WriteEncodeInt(offset);
	ms.Write(bs);
	msg.Length	= ms.Position();

	TokenMessage tmsg;
	TinyToToken(msg,tmsg);
	return TkClient->Send(tmsg);
	//return Send(msg);
}
