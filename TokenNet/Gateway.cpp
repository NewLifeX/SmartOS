#include "Time.h"
#include "Gateway.h"
#include "Config.h"

#include "Security\MD5.h"

bool OnLocalReceived(Message& msg, void* param);
bool OnRemoteReceived(Message& msg, void* param);

bool TokenToTiny(const TokenMessage& msg, TinyMessage& msg2);
void TinyToToken(const TinyMessage& msg, TokenMessage& msg2);
void OldTinyToToken(const TinyMessage& msg, TokenMessage& msg2,  ushort kind);
void OldTinyToToken0x10(const TinyMessage& msg, TokenMessage& msg2);
void OldTinyToToken0x11(const TinyMessage& msg, TokenMessage& msg2);
void OldTinyToToken0x12(const TinyMessage& msg, TokenMessage& msg2);

// 本地网和远程网一起实例化网关服务
Gateway::Gateway()
{
	Server	= NULL;
	Client	= NULL;
	Led		= NULL;

	Running		= false;
	AutoReport	= false;
	IsOldOrder	= false;
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

	Server->Received	= OnLocalReceived;
	Server->Param		= this;
	Server->Student		= Student;
	
	Client->Received	= OnRemoteReceived;
	Client->Param		= this;
    Client->IsOldOrder  = IsOldOrder;

	debug_printf("Gateway::Start \r\n");

	// 添加网关这一条设备信息
	if(Server->Devices.Count() == 0)
	{
		Device* dv = new Device();
		dv->Address		= Server->Config->Address;
		dv->Kind		= Sys.Code;
		dv->HardID.SetLength(16);
		dv->HardID		= Sys.ID;
		dv->LastTime	= Time.Current();
		dv->Name		= Sys.Name;

		Server->Devices.Add(dv);
	}

	Server->Start();
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

// 本地网收到设备端消息
bool OnLocalReceived(Message& msg, void* param)
{
	Gateway* server = (Gateway*)param;
	assert_ptr(server);

	return server->OnLocal((TinyMessage&)msg);
}

// 远程网收到服务端消息
bool OnRemoteReceived(Message& msg, void* param)
{
	Gateway* server = (Gateway*)param;
	assert_ptr(server);

	return server->OnRemote((TokenMessage&)msg);
}

// 数据接收中心
bool Gateway::OnLocal(const TinyMessage& msg)
{
	// 本地处理。注册、上线、同步时间等
	/*switch(msg.Code)
	{
		case 0x01:
			return OnDiscover(msg);
	}*/
	Device* dv = Server->Current;
	if(dv)
	{
		// 短时间内注册或者登录
		ulong now = Time.Current() - 500;
		if(dv->RegTime > now) DeviceRegister(dv->Address);
		if(dv->LoginTime > now) DeviceOnline(dv->Address);
	}

	// 消息转发
	if(msg.Code >= 0x10 && msg.Dest == Server->Config->Address)
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
				//Sys.Sleep(1000);
				TokenMessage rs;
				rs.Code = 0x21;
				OnGetDeviceList(rs);
				// 遍历发送所有设备信息
				for(int i=0; i<Server->Devices.Count(); i++)
					SendDeviceInfo(Server->Devices[i]);
			}
			break;

		case 0x20:
			return OnMode(msg);
		case 0x21:
			return OnGetDeviceList(msg);
		case 0x25:
			return OnGetDeviceInfo(msg);
	    case 0x26:
           OnDeviceDelete(msg);
		   break;
	}

	// 消息转发
	if(msg.Code >= 0x10 && !msg.Error && msg.Length < 25)
	{
		//debug_printf("Gateway::Remote ");
		msg.Show();

		TinyMessage tmsg;
		if(!TokenToTiny(msg, tmsg)) return true;

		bool rs = Server->Dispatch(tmsg);
		if(rs)
		{
		    debug_printf("rs = Server->Dispatch(tmsg)\r\n");

			TokenMessage msg2;

			if(IsOldOrder)
		    {
				Device* dv1 = Server->FindDevice(tmsg.Src);

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
		    }
	        else
		       TinyToToken(tmsg, msg2);
			// TinyToToken(tmsg, msg2);
			return Client->Reply(msg2);
		}
	}

	return true;
}

// 设备列表 0x21
bool Gateway::OnGetDeviceList(const Message& msg)
{
	if(msg.Reply) return false;

	// 不管请求内容是什么，都返回设备ID列表
	TokenMessage rs;
	rs.Code = msg.Code;

	if(Server->Devices.Count() == 0)
	{
		rs.Data[0] = 0;
		rs.Length = 1;
	}
	else
	{
		int i = 0;
		for(i=0; i<Server->Devices.Count(); i++)
			rs.Data[i] = Server->Devices[i]->Address;
		rs.Length = i;
	}

    debug_printf("获取设备列表 共%d个\r\n", Server->Devices.Count());

	return Client->Reply(rs);
}

// 设备信息 0x25
bool Gateway::OnGetDeviceInfo(const Message& msg)
{
	if(msg.Reply) return false;

	// 如果没有给ID，那么返回空负载
	if(msg.Length == 0) return Client->Reply((TokenMessage&)msg);

	TokenMessage rs;
	rs.Code = msg.Code;

	// 如果未指定设备ID，则默认为1，表示网关自身
	byte id = 1;
	if(msg.Length > 0) id = msg.Data[0];
	debug_printf("获取节点信息 ID=0x%02X\r\n", id);

	// 找到对应该ID的设备
	Device* dv = Server->FindDevice(id);

	// 即使找不到设备，也返回空负载数据
	if(!dv) return Client->Reply(rs);

	dv->Show(true);
	
	 //旧指令的开个位先用数据长度替代
	 if(IsOldOrder)
	 {
		//dv->DataSize=dv->Store[0];//数据区的第一长度为主数据区长度		 
		//dv->ConfigSize=2;		 
	 }

	return SendDeviceInfo(dv);
}

// 发送设备信息
bool Gateway::SendDeviceInfo(const Device* dv)
{
	if(!dv) return false;

   
	TokenMessage rs;
	rs.Code = 0x25;
	// 担心rs.Data内部默认缓冲区不够大，这里直接使用数据流。必须小心，ms生命结束以后，它的缓冲区也将无法使用
	//Stream ms(rs.Data, rs.Length);
	MemoryStream ms;

	dv->Write(ms);

	// 当前作用域，直接使用数据流的指针，内容可能扩容而导致指针改变
	rs.Data		= ms.GetBuffer();
	rs.Length	= ms.Position();

	return Client->Reply(rs);
}

void ExitStudentMode(void* param)
{ 
	Gateway* gw = (Gateway*)param;
	gw->SetMode(false);
}

// 学习模式 0x20
void Gateway::SetMode(bool student)
{
	Student  		= student;
	Server->Student = Student;

	// 设定小灯快闪时间，单位毫秒
	if(Led) Led->Write(student ? 90000 : 100);

	TokenMessage msg;
	msg.Code	= 0x20;
	msg.Length	= 1;
	msg.Data[0]	= student ? 1 : 0;
	debug_printf("%s 学习模式\r\n", student ? "进入" : "退出");

	// 定时退出学习模式
	if(student)
	{		
		Sys.AddTask(ExitStudentMode, this, 90000, -1, "退出学习");
	}

	Client->Send(msg);
}

bool Gateway::OnMode(const Message& msg)
{
	bool rs = msg.Data[0] != 0;
	SetMode(rs);

	return true;
}

// 节点注册入网 0x22
void Gateway::DeviceRegister(byte id)
{
	if(!Student) return;
	
	TokenMessage msg;
	msg.Code	= 0x22;
	msg.Length	= 1;
	msg.Data[0]	= id;
	debug_printf("节点注册入网 ID=0x%02X\r\n", id);

	Client->Send(msg);
	if(AutoReport)
	{
		Device* dv = Server->FindDevice(id);
		SendDeviceInfo(dv);
	}
}

// 节点上线 0x23
void Gateway::DeviceOnline(byte id)
{
	TokenMessage msg;
	msg.Code	= 0x23;
	msg.Length	= 1;
	msg.Data[0]	= id;
	debug_printf("节点上线 ID=0x%02X\r\n", id);

	Client->Send(msg);
	if(AutoReport)
	{
		Device* dv = Server->FindDevice(id);
		SendDeviceInfo(dv);
	}
}

// 节点离线 0x24
void Gateway::DeviceOffline(byte id)
{
	TokenMessage msg;
	msg.Code	= 0x24;
	msg.Length	= 1;
	msg.Data[0]	= id;
	debug_printf("节点离线 ID=0x%02X\r\n", id);

	Client->Send(msg);
	if(AutoReport)
	{
		Device* dv = Server->FindDevice(id);
		SendDeviceInfo(dv);
	}
}

// 节点删除 0x26
void Gateway::OnDeviceDelete(const Message& msg)
{
	if(msg.Reply) return;
	if(msg.Length == 0) return;

	byte id = msg.Data[0];

	TokenMessage rs;
	rs.Code = msg.Code;

	debug_printf("节点删除 ID=0x%02X\r\n", id);

	bool success = Server->DeleteDevice(id);

	rs.Length	= 1;
	rs.Data[0]	= success ? 0 : 1;

	Client->Reply(rs);

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
