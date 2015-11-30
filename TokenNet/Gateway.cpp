#include "Gateway.h"
#include "Config.h"

#include "Security\MD5.h"

bool TokenToTiny(const TokenMessage& msg, TinyMessage& msg2);
void TinyToToken(const TinyMessage& msg, TokenMessage& msg2);
void OldTinyToToken(const TinyMessage& msg, TokenMessage& msg2,  ushort kind);
void OldTinyToToken0x10(const TinyMessage& msg, TokenMessage& msg2);
void OldTinyToToken0x11(const TinyMessage& msg, TokenMessage& msg2);
void OldTinyToToken0x12(const TinyMessage& msg, TokenMessage& msg2);

Gateway* Gateway::Current	= NULL;

// 本地网和远程网一起实例化网关服务
Gateway::Gateway()
{
	Server	= NULL;
	Client	= NULL;
	Led		= NULL;

	Running		= false;
	AutoReport	= false;
	IsOldOrder	= false;
	_taskStudy	= 0;
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

	assert_param2(Server, "微网服务端未设置");
	assert_param2(Client, "令牌客户端未设置");

	Server->Received	= [](void* s, Message& msg, void* p){ return ((Gateway*)p)->OnLocal((TinyMessage&)msg); };
	Server->Param		= this;
	Server->Study		= Study;

	Client->Received	= [](void* s, Message& msg, void* p){ return ((Gateway*)p)->OnRemote((TokenMessage&)msg); };
	Client->Param		= this;
    Client->IsOldOrder  = IsOldOrder;

	debug_printf("\r\nGateway::Start \r\n");

	Server->Start();

	// 添加网关这一条设备信息
	if(Server->Devices.Length() == 0)
	{
		auto dv = new Device();
		dv->Address		= Server->Cfg->Address;
		dv->Kind		= Sys.Code;
		dv->HardID.Copy(Sys.ID, 16);
		dv->LastTime	= Sys.Seconds();
		dv->Name		= Sys.Name;

		Server->Devices.Push(dv);
		Server->SaveDevices();
	}

	Client->Open();

	Running = true;
}

// 停止网关。取消本地和远程的消息挂载
void Gateway::Stop()
{
	if(!Running) return;

	Server->Received	= NULL;
	Server->Param		= NULL;
	Client->Received	= NULL;
	Client->Param		= NULL;

	Running = false;
}

// 数据接收中心
bool Gateway::OnLocal(const TinyMessage& msg)
{
	auto dv = Server->Current;
	if(dv)
	{
		// 短时间内注册或者登录
		ulong now = Sys.Ms() - 500;
		if(dv->RegTime > now) DeviceRequest(DeviceAtions::Register, dv);
		if(dv->LoginTime > now) DeviceRequest(DeviceAtions::Online, dv);
	}

	// 消息转发
	if(msg.Code >= 0x10 && msg.Dest == Server->Cfg->Address)
	{
		TokenMessage tmsg;

		if(IsOldOrder && dv)
		{
			switch(dv->Kind)
			{
				/// <summary>触摸开关(1位)</summary>
				case 0x0201:
				/// <summary>触摸开关(2位)</summary>
				case 0x0202:
				/// <summary>触摸开关(3位)</summary>
				case 0x0203:
				/// <summary>触摸开关(4位)</summary>
				case 0x0204:
				/// <summary>情景面板(1位)</summary>
				case 0x0211:
				/// <summary>情景面板(2位)</summary>
				case 0x0212:
				/// <summary>情景面板(3位)</summary>
				case 0x0213:
				/// <summary>情景面板(4位)</summary>
				case 0x0214:
				/// <summary>智能继电器(1位)</summary>
				case 0x0221:
				/// <summary>智能继电器(2位)</summary>
				case 0x0222:
				/// <summary>智能继电器(3位)</summary>
				case 0x0223:
				/// <summary>智能继电器(4位)</summary>
				case 0x0224:
				/// <summary>单火线取电开关(1位)</summary>
				case 0x0231:
				/// <summary>单火线取电开关(2位)</summary>
				case 0x0232:
				/// <summary>单火线取电开关(位)</summary>
				case 0x0233:
				/// <summary>单火线取电开关(4位)</summary>
				case 0x0234:
				/// <summary>无线遥控插座(1位)</summary>
				case 0x0261:
				/// <summary>无线遥控插座(2位)</summary>
				case 0x0262:
				/// <summary>无线遥控插座(3位)</summary>
				case 0x0263:
				/// <summary>智能门锁</summary>
				case 0x0411:
				/// <summary>机械手</summary>
				case 0x0421:
				/// <summary>电动窗帘</summary>
				case 0x0431:
				/// <summary>门窗磁</summary>
				case 0x0531:
				/// <summary>红外感应器</summary>
				case 0x0541:
				/// <summary>摄像头</summary>
				case 0x0551:
				/// <summary>声光报警器</summary>
				case 0x0561:
					OldTinyToToken10(msg, tmsg);
				break;
				default:
					OldTinyToToken(msg, tmsg, dv->Kind);
				break;
			}
		}
	    else
		   TinyToToken(msg, tmsg);

		Client->Send(tmsg);
	}

	return true;
}

bool Gateway::OnRemote(const TokenMessage& msg)
{
	// 本地处理
	switch(msg.Code)
	{
		case 0x02:
			// 登录以后自动发送设备列表和设备信息
			if(AutoReport && msg.Reply && Client->Token != 0)
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

	// 消息转发
	if(msg.Code >= 0x10 && !msg.Error && msg.Length < 25)
	{
		//debug_printf("Gateway::Remote ");
		msg.Show();

		TinyMessage tmsg;
		if(!TokenToTiny(msg, tmsg)) return true;

		bool rs = Server->Dispatch(tmsg);
		if(rs) return false;

		TokenMessage msg2;

		if(IsOldOrder)
		{
			auto dv1 = Server->FindDevice(tmsg.Src);

			switch(dv1->Kind)
			{
				/// <summary>触摸开关(1位)</summary>
				case 0x0201:
				/// <summary>触摸开关(2位)</summary>
				case 0x0202:
				/// <summary>触摸开关(3位)</summary>
				case 0x0203:
				/// <summary>触摸开关(4位)</summary>
				case 0x0204:
				/// <summary>情景面板(1位)</summary>
				case 0x0211:
				/// <summary>情景面板(2位)</summary>
				case 0x0212:
				/// <summary>情景面板(3位)</summary>
				case 0x0213:
				/// <summary>情景面板(4位)</summary>
				case 0x0214:
				/// <summary>智能继电器(1位)</summary>
				case 0x0221:
				/// <summary>智能继电器(2位)</summary>
				case 0x0222:
				/// <summary>智能继电器(3位)</summary>
				case 0x0223:
				/// <summary>智能继电器(4位)</summary>
				case 0x0224:
				/// <summary>单火线取电开关(1位)</summary>
				case 0x0231:
				/// <summary>单火线取电开关(2位)</summary>
				case 0x0232:
				/// <summary>单火线取电开关(位)</summary>
				case 0x0233:
				/// <summary>单火线取电开关(4位)</summary>
				case 0x0234:
				/// <summary>无线遥控插座(1位)</summary>
				case 0x0261:
				/// <summary>无线遥控插座(2位)</summary>
				case 0x0262:
				/// <summary>无线遥控插座(3位)</summary>
				case 0x0263:
				/// <summary>智能门锁</summary>
				case 0x0411:
				/// <summary>机械手</summary>
				case 0x0421:
				/// <summary>电动窗帘</summary>
				case 0x0431:
				/// <summary>门窗磁</summary>
				case  0x0531:
				/// <summary>红外感应器</summary>
				case 0x0541:
				/// <summary>摄像头</summary>
				case 0x0551:
				/// <summary>声光报警器</summary>
				case 0x0561:
					OldTinyToToken10(tmsg, msg2);
				break;
				default:
					OldTinyToToken(tmsg, msg2, dv1->Kind);
				break;
		    }
			// TinyToToken(tmsg, msg2);
		}
		 else
		      TinyToToken(tmsg, msg2);

		return Client->Reply(msg2);
	}

	return true;
}

// 设备列表 0x21
bool Gateway::SendDevices(DeviceAtions act, const Device* dv)
{
	TokenMessage msg;
	msg.Code = 0x21;

	int count = Server->Devices.Length();
	if(dv) count	= 1;

	byte buf[1500];		// 1024 字节只能承载 23条数据，udp最大能承载1514字节
	MemoryStream ms(buf, ArrayLength(buf));
	//MemoryStream ms(1536);
	ms.Write((byte)act);
	ms.Write((byte)count);
	//ms.WriteEncodeInt(count);

	if(count > 0)
	{
		if(dv)
			dv->Write2(ms);
		else
		{
			for(int i=0; i<count; i++)
				Server->Devices[i]->Write2(ms);
		}
	}

	msg.Length 	= ms.Position();
	msg.Data 	= ms.GetBuffer(); 

    debug_printf("发送设备列表 共%d个\r\n", count);

	if(act == DeviceAtions::List)
		return Client->Reply(msg);
	else
		return Client->Send(msg);
}

/*void ExitStudentMode(void* param)
{
	Gateway* gw = (Gateway*)param;
	gw->SetMode(false);
}*/

// 学习模式 0x20
void Gateway::SetMode(bool study)
{
	TS("Gateway::SetMode");

	Study  		  = study;
	Server->Study = Study;

	// 设定小灯快闪时间，单位毫秒
	if(Led) Led->Write(study ? 90000 : 100);

	TokenMessage msg;
	msg.Code	= 0x20;
	msg.Length	= 1;
	msg.Data[0]	= study ? 2 : 0;
	//msg.Data[1]	= study ? 1 : 0;

	debug_printf("%s 学习模式\r\n", study ? "进入" : "退出");

	// 定时退出学习模式
	if(study)
	{
		//Sys.AddTask(ExitStudentMode, this, 90000, -1, "退出学习");
		if(_taskStudy)
			Sys.SetTask(_taskStudy, true, 90000);
		else
			_taskStudy = Sys.AddTask([](void* p){ ((Gateway*)p)->SetMode(false); }, this, 90000, -1, "退出学习");
	}
	else
	{
		if(_taskStudy)
			Sys.SetTask(_taskStudy, false);
	}

	Client->Reply(msg);
}

// 学习模式 0x20
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
    if(msg.Length<1)
    {
    	SetMode(true);
         return true;
    }
    //自动学习模式
    if(msg.Data[0]==2)
    {
        SetMode(true);
        return true;

    }

     //手动学习模式
   if(msg.Data[0]==1)
    {
       Study  	  = true;
       Server->Study = Study;

      // 设定小灯快闪时间，单位毫秒
       if(Led) Led->Write(900000);

      TokenMessage msg;
      msg.Code	= 0x20;
      msg.Length	= 1;
      msg.Data[0]	= 1;
     // msg.Data[1] = 1;
      debug_printf("%s 学习模式\r\n", Study ? "进入" : "退出");
	  Client->Reply(msg);
     }

     //退出学习模式
     if(msg.Data[0]==0)
     {
      SetMode(false);
      return true;

     }

	  return true;
}

// 节点消息处理 0x21
void Gateway::DeviceRequest(DeviceAtions act, const Device* dv)
{
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
			debug_printf("节点删除 ID=0x%02X\r\n", id);
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

	Client->Send(rs);
}

bool Gateway::DeviceProcess(const Message& msg)
{
	// 仅处理来自云端的请求
	if(msg.Reply) return false;

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

				dv->Read2(ms);
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
			debug_printf("节点删除 ID=0x%02X\r\n", id);
		{
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

void Gateway::OldTinyToToken10(const TinyMessage& msg, TokenMessage& msg2)
{
	TinyMessage tmsg;
	//交换目标和源地址
	tmsg.Code	= 0x15;
	tmsg.Src	= msg.Dest;
	tmsg.Dest	= msg.Src;
	//tmsg.Sequence=msg.Sequence+1;

	tmsg.Length	= 2;
	tmsg.Data[0]	= 1;
	tmsg.Data[1]	= 6;

	//tmsg.Length=msg.Length;
	//if(msg.Length > 0) memcpy(&tmsg.Data, msg.Data, msg.Length);
	// debug_printf(" 微网10指令转换\r\n");
	tmsg.Show();

	// bool rs = Server->Dispatch(tmsg);
	//  debug_printf("2微网10指令转换\r\n");
	Device* dv = Server->FindDevice(tmsg.Dest);
	bool rs = Server->OnRead(tmsg, *dv);

	if(rs)
	{
		tmsg.Dest	= tmsg.Src;
		tmsg.Src	= dv->Address;
		tmsg.Show();
	}

	if(rs)
	{
		msg2.Code	= 0x10;
		msg2.Data[0]	= tmsg.Src;

		msg2.Length	= msg.Length + 8;

		if(msg.Length > 0) memcpy(&msg2.Data[1], &tmsg.Data[1], tmsg.Length);

		msg2.Reply	= tmsg.Reply;
		msg2.Error	= tmsg.Error;

		// debug_printf("指令转换\r\n");
		msg2.Show();
	}
}

bool TokenToTiny(const TokenMessage& msg, TinyMessage& tny)
{
	if(msg.Length == 0) return false;

	// 处理Reply标记
	tny.Reply	= msg.Reply;
	tny.Error	= msg.Error;

	// 第一个字节是节点设备地址
	tny.Dest	= msg.Data[0];

	switch(msg.Code)
	{
		case 0x10:
			tny.Code	= 0x16;
			tny.Length	= msg.Length;

			if(msg.Length > 1) memcpy(&tny.Data[1], &msg.Data[1], msg.Length - 1);
			// 从偏移1开始，数据区偏移0是长度
			tny.Data[0]	= 1;

			break;
		case 0x11:
			tny.Code	= 0x15;

			// 通道号*4-4为读取的起始地址
			tny.Data[0]	= 4 * (msg.Data[1] - 1) + 1;
			tny.Data[1]	= msg.Data[2] * 4;

			tny.Length	= 2;
			break;
		case 0x12:
			tny.Code	= 0x16;
			tny.Length	= msg.Length - 1;
			tny.Data[0]	=(msg.Data[1] - 1) * 4 + 1;

			if(msg.Length > 2) memcpy(&tny.Data[1], &msg.Data[2], msg.Length);//去掉通道号

			break;
        default:
			tny.Code	= msg.Code;
			if(msg.Length > 1) memcpy(tny.Data, &msg.Data[1], msg.Length - 1);
			tny.Length	= msg.Length - 1;

			break;
	}

	return true;
}

void TinyToToken(const TinyMessage& msg, TokenMessage& msg2)
{
	// 处理Reply标记
	msg2.Code = msg.Code;
	msg2.Reply = msg.Reply;
	msg2.Error = msg.Error;
	// 第一个字节是节点设备地址
	msg2.Data[0] = ((TinyMessage&)msg).Src;

	if(msg.Length > 0) memcpy(&msg2.Data[1], msg.Data, msg.Length);

	msg2.Length = 1 + msg.Length;
}

void OldTinyToToken0x10(const TinyMessage& msg, TokenMessage& msg2)
{
	if(msg.Code == 0x16)
	{
		msg.Data[2] = 5;
	}

	msg2.Code	= 0x10;
	msg2.Data[0]	= ((TinyMessage&)msg).Src;

	msg2.Length	= msg.Length+1;

	if(msg.Length > 0) memcpy(&msg2.Data[1], msg.Data, msg.Length);

	msg2.Reply	= msg.Reply;
	msg2.Error	= msg.Error;
}

void OldTinyToToken0x11(const TinyMessage& msg, TokenMessage& msg2)
{
	msg2.Code	= 0x11;
	msg2.Length	= msg.Length+1;

	msg2.Data[0]	= ((TinyMessage&)msg).Src;

	if(msg.Length > 0) memcpy(&msg2.Data[1], msg.Data, msg.Length);

	int i = (msg.Data[0]-1)/4+1;//起始地址-1除4+1得通道号

	msg2.Data[1]	= i;//Data[1]修改通道号

	msg2.Reply	= msg.Reply;
	msg2.Error	= msg.Error;

	// debug_printf(" 11指令转换\r\n");
	// msg2.Show();
}

void OldTinyToToken0x12(const TinyMessage& msg, TokenMessage& msg2)
{
	msg2.Code	= 0x12;
	msg2.Length	= msg.Length + 1;

	msg2.Data[0]	= msg.Src;
	int i=(msg.Data[0]-1)/4+1;//起始地址-1除4+1得通道号

	msg2.Data[1]	= i;//Data[1]修改通道号


	if(msg.Length > 0) memcpy(&msg2.Data[1], msg.Data, msg.Length);

	msg2.Reply	= msg.Reply;
	msg2.Error	= msg.Error;

	// debug_printf(" 12指令转换\r\n");
	// msg2.Show();
}

void OldTinyToToken(const TinyMessage& msg, TokenMessage& msg2, ushort kind)
{
	// 处理Reply标记

	//  debug_printf(" 指令转换:0x%02X\r\n",kind);
	// msg2.Show();
	switch(kind)
	{
		case 0x0101:
		/// <summary>网关B</summary>
		case 0x0102:
		/// <summary>网关C</summary>
		case 0x0103:
		/// <summary>无线中继</summary>
		case 0x01C8:
			TinyToToken(msg,msg2);
			break;
		/// <summary>环境探测器</summary>
		case 0x0311:
		/// <summary>PM2.5检测器</summary>
		case 0x0321:
		/// <summary>红外转发器</summary>
		case 0x0331:
		/// <summary>调光开关</summary>
		case 0x0241:
		/// <summary>调色开关</summary>
		case  0x0251:
		/// <summary>燃气探测器</summary>
		case 0x0501:
		/// <summary>火焰烟雾探测器</summary>
		case 0x0521:
		/// <summary>温控面板</summary>
		case 0x0601:
		/// <summary>调色控制器</summary>
		case 0x0611:
		/// <summary>背景音乐控制器</summary>
		case 0x0621:
			if(msg.Code == 0x15)
				OldTinyToToken0x11(msg, msg2);
			else
				OldTinyToToken0x12(msg, msg2);
			break;
		default:
			TinyToToken(msg,msg2);
		break;
	}
}

Gateway* Gateway::CreateGateway(TokenClient* client, TinyServer* server)
{
	debug_printf("\r\nGateway::CreateGateway \r\n");

	Gateway* gw	= Current;
	if(gw)
	{
		if((client == NULL || gw->Client == client) &&
		(server == NULL || gw->Server == server)) return gw;

		delete gw;
	}

	gw = new Gateway();
	gw->Client	= client;
	gw->Server	= server;

	Current		= gw;

	return gw;
}
