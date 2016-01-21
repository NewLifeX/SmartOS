#include "Gateway.h"
#include "Config.h"

#include "Security\MD5.h"
#include "Security\Crc.h"

// 循环间隔
#define LOOP_Interval	10000

bool TokenToTiny(const TokenMessage& msg, TinyMessage& msg2);
void TinyToToken(const TinyMessage& msg, TokenMessage& msg2);
Gateway* Gateway::Current	= NULL;

// 本地网和远程网一起实例化网关服务
Gateway::Gateway()
{
	Server	= NULL;
	Client	= NULL;
	Led		= NULL;

	Running		= false;
}

Gateway::~Gateway()
{
	Stop();

	delete Server;
	Server = NULL;

	delete Client;
	Client = NULL;
}

// 启动网关。挂载本地和远程的消息事件
void Gateway::Start()
{
	if(Running) return;

	TS("Gateway::Start");

	assert_param2(Server, "微网服务端未设置");
	assert_param2(Client, "令牌客户端未设置");

	Server->Received	= [](void* s, Message& msg, void* p){ return ((Gateway*)p)->OnLocal((TinyMessage&)msg); };
	Server->Param		= this;

	Client->Received	= [](void* s, Message& msg, void* p){ return ((Gateway*)p)->OnRemote((TokenMessage&)msg); };
	Client->Param		= this;

	debug_printf("\r\nGateway::Start \r\n");

	Server->Start();

	// 添加网关这一条设备信息
	if(Server->Devices.Length() == 0)
	{
		auto dv = new Device();
		dv->Address		= Server->Cfg->Address;
		dv->Kind		= Sys.Code;
		dv->LastTime	= Sys.Seconds();

		dv->SetHardID(Array(Sys.ID, 16));
		dv->SetName(Array(Sys.Name, 0));

		Server->Devices.Push(dv);
		Server->SaveDevices();
	}

	Client->Open();
	_task	= Sys.AddTask(Loop, this, 10000, LOOP_Interval, "设备任务");

	Running = true;
}

// 停止网关。取消本地和远程的消息挂载
void Gateway::Stop()
{
	if(!Running) return;

	Sys.RemoveTask(_task);

	Server->Received	= NULL;
	Server->Param		= NULL;
	Client->Received	= NULL;
	Client->Param		= NULL;

	Running = false;
}

// 数据接收中心
bool Gateway::OnLocal(const TinyMessage& msg)
{
	TS("Gateway::OnLocal");

	auto dv = Server->Current;
	if(dv)
	{
		switch(msg.Code)
		{
			case 0x01:
				DeviceRequest(DeviceAtions::Register, dv);
				DeviceRequest(DeviceAtions::Online, dv);
				break;
			case 0x02:
				DeviceRequest(DeviceAtions::Delete, dv);
				break;				
		}
	}

	// 应用级消息转发
	if(msg.Code >= 0x10 && msg.Dest == Server->Cfg->Address)
	{
		TokenMessage tmsg;

		TinyToToken(msg, tmsg);

		Client->Send(tmsg);
	}

	return true;
}

bool Gateway::OnRemote(const TokenMessage& msg)
{
	TS("Gateway::OnRemote");

	switch(msg.Code)
	{
		case 0x02:
			// 登录以后自动发送设备列表和设备信息
			if(msg.Reply && Client->Token != 0)
			{
				// 遍历发送所有设备信息
				SendDevices(DeviceAtions::List, NULL);
			}
			break;

		case 0x20:
			return OnMode(msg);
		case 0x21:
			return DeviceProcess(msg);
	}

	// 应用级消息转发
	if(msg.Code >= 0x10 && !msg.Error && msg.Length <= Server->Control->Port->MaxSize - TinyMessage::MinSize)
	{
		//debug_printf("Gateway::Remote ");
		//msg.Show();

		TinyMessage tmsg;
		if(!TokenToTiny(msg, tmsg)) return true;

		bool rs = Server->Dispatch(tmsg);
		if(!rs) return false;

		TokenMessage msg2;
		TinyToToken(tmsg, msg2);

		//msg2.Show();
		return Client->Reply(msg2);
	}

	return true;
}

// 设备列表 0x21
bool Gateway::SendDevices(DeviceAtions act, const Device* dv)
{
	TS("Gateway::SendDevices");

	if(Client->Status < 2) return false;

	TokenMessage msg;
	msg.Code = 0x21;

	int count = Server->Devices.Length();
	if(dv) count	= 1;

	byte buf[1500];		// 1024 字节只能承载 23条数据，udp最大能承载1514字节
	MemoryStream ms(buf, ArrayLength(buf));
	//MemoryStream ms(1536);
	ms.Write((byte)act);
	ms.Write((byte)count);

	if(count > 0)
	{
		if(dv)
			dv->WriteMessage(ms);
		else
		{
			for(int i=0; i<count; i++)
				Server->Devices[i]->WriteMessage(ms);
		}
	}

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
		return Client->Reply(msg);
	else
		return Client->Send(msg);
}

void Gateway::SendDevicesIDs()
{
	TokenMessage msg;
	msg.Code = 0x21;	
	auto act=DeviceAtions::ListIDs;
	
	MemoryStream ms;
	ms.Write((byte)act);
	byte len = Server->Devices.Length();
	//先确认真正的设备有多少个
	for(int i = 0; i < len; i++)
	{
		auto dv =Server->Devices[i];		
		if(!dv) continue;		
		ms.Write(dv->Address);	
	}
	
	msg.Length 	= ms.Position();
	msg.Data 	= ms.GetBuffer();
	
	Client->Send(msg);	
}
// 学习模式 0x20
void Gateway::SetMode(uint sStudy)
{
	TS("Gateway::SetMode");

	Server->Study = sStudy > 0;

	// 兼容旧版本
	switch(sStudy)
	{
		case 1:
			sStudy	= 30;
			break;
		case 2:
			sStudy	= 90;
			break;
	}

	// 定时退出学习模式
	_Study	= sStudy;

	// 设定小灯快闪时间，单位毫秒
	if(Led) Led->Write(sStudy ? sStudy * 1000 : 100);

	if(sStudy)
		debug_printf("进入 学习模式 %d 秒\r\n", sStudy);
	else
		debug_printf("退出 学习模式\r\n");

	TokenMessage msg;
	msg.Code	= 0x20;
	msg.Length	= 1;
	msg.Data[0]	= sStudy;

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
	msg.Code	= 0x35;
	msg.Length	= 1;
	msg.Data[0]	= 0;
	Client->Reply(msg);
}

bool Gateway::OnMode(const Message& msg)
{
	msg.Show();

	TS("Gateway::OnMode");

    if(msg.Length < 1)
    {
    	SetMode(30);
        return true;
    }

    // 自动学习模式
	SetMode(msg.Data[0]);

	return true;
}

// 节点消息处理 0x21
void Gateway::DeviceRequest(DeviceAtions act, const Device* dv)
{
	TS("Gateway::DeviceRequest");

	if(Client->Status < 2) return;

	byte id	= dv->Address;
	switch(act)
	{
		case DeviceAtions::List:
			SendDevices(act, dv);
			return;
		case DeviceAtions::Update:
			SendDevices(act, dv);
			return;
		case DeviceAtions::Register:
			debug_printf("节点注册入网 ID=0x%02X\r\n", id);
			SendDevices(act, dv);
			return;
		case DeviceAtions::Online:
			debug_printf("节点上线 ID=0x%02X\r\n", id);
			break;
		case DeviceAtions::Offline:
			debug_printf("节点离线 ID=0x%02X\r\n", id);
			break;
		case DeviceAtions::Delete:
		{
			debug_printf("节点删除~~ ID=0x%02X\r\n", id);
			auto dv = Server->FindDevice(id);
			if(dv == NULL) return;
			Server->DeleteDevice(id);			
			break;
		}
		default:
			debug_printf("无法识别的节点操作 Act=%d ID=0x%02X\r\n", (byte)act, id);
			break;
	}

	TokenMessage rs;
	rs.Code	= 0x21;
	rs.Length	= 2;
	rs.Data[0]	= (byte)act;
	rs.Data[1]	= id;

	Client->Send(rs);
}

bool Gateway::DeviceProcess(const Message& msg)
{
	// 仅处理来自云端的请求
	if(msg.Reply) return false;

	TS("Gateway::DeviceProcess");

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
			SendDevices(act, NULL);
			return true;
		}
		case DeviceAtions::Update:
		{
			// 云端要更新网关设备信息
			auto dv = Server->FindDevice(id);
			if(!dv)
			{
				rs.Error	= true;
			}
			else
			{
				auto ms	= msg.ToStream();
				ms.Seek(2);

				dv->ReadMessage(ms);
				Server->SaveDevices();
			}

			Client->Reply(rs);
		}
			break;
		case DeviceAtions::Register:
			break;
		case DeviceAtions::Online:
			break;
		case DeviceAtions::Offline:
			break;
		case DeviceAtions::Delete:
			debug_printf("节~~1点删除 ID=0x%02X\r\n", id);
		{
			auto dv = Server->FindDevice(id);
			if(dv == NULL) 
			{
				rs.Error = true;				
				Client->Reply(rs);
				return false;
			}
			
			ushort crc = Crc::Hash16(dv->GetHardID());
			Server->Disjoin(id);
			Sys.Sleep(300);
			Server->Disjoin(id);
			Sys.Sleep(300);
			Server->Disjoin(id);
			// 云端要删除本地设备信息
			bool flag = Server->DeleteDevice(id);
			rs.Error	= !flag;
			Client->Reply(rs);			
		}
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
	if(msg.Length > 1) memcpy(tny.Data, &msg.Data[1], msg.Length - 1);
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

	if(msg.Length > 0) memcpy(&msg2.Data[1], msg.Data, msg.Length);

	msg2.Length = 1 + msg.Length;
}

Gateway* Gateway::CreateGateway(TokenClient* client, TinyServer* server)
{
	debug_printf("\r\nGateway::CreateGateway \r\n");

	Gateway* gw	= Current;
	if(gw)
	{
		if(	(client == NULL || gw->Client == client) &&
			(server == NULL || gw->Server == server)) return gw;

		delete gw;
	}

	gw = new Gateway();
	gw->Client	= client;
	gw->Server	= server;

	Current		= gw;

	return gw;
}

// 设备上线下线报备
void Gateway::Loop(void* param)
{
	TS("Gateway::Loop");

	auto gw		= 	(Gateway*)param;
	
	gw->SendDevicesIDs();
	auto now	= Sys.Seconds();
	byte len	= gw->Server->Devices.Length();
	for(int i = 0; i < len; i++)
	{
		auto dv = gw->Server->Devices[i];
		
		if(!dv) continue;
		
		ushort time = dv->OfflineTime ? dv->OfflineTime :60;

		// 特殊处理网关自身
		if(dv->Address == gw->Server->Cfg->Address) dv->LastTime = now;

		if(dv->LastTime + time < now)
		{	// 下线			
			if(dv->Logined)
			{	
				//debug_printf("设备最后活跃时间：%d,系统当前时间:%d,离线阈值:%d\r\n",dv->LastTime,now,time);
				gw->DeviceRequest(DeviceAtions::Offline, dv);
				dv->Logined = false;
			}
		}
		else
		{	// 上线
			if(!dv->Logined)
			{				
				gw->DeviceRequest(DeviceAtions::Online, dv);
				dv->Logined = true;
			}
		}
	}
}
