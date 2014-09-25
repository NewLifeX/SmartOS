#include "Message.h"

// 初始化消息，各字段为0
void Message::Init()
{
	memset(this, 0, sizeof(Message));
}

// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
bool Message::Parse(byte* buf, uint len)
{
	assert_ptr(buf);
	assert_param(len > 0);

	// 消息至少4个头部字节和2个校验字节，没有负载数据的情况下
	const int headerSize = 4 + 2;
	if(len < headerSize) return false;

	// 复制头4字节
	*(uint*)this = *(uint*)buf;

	Crc16 = Sys.Crc16(buf, len - 2);

	if(len > headerSize)
	{
		Length = len - headerSize;
		Data = buf + 4;
	}
	else
	{
		Length = 0;
		Data = NULL;
	}

	// 数据校验在最后2字节
	Checksum = *(ushort*)(buf + len - 2);

	return true;
}

// 验证消息校验和是否有效
bool Message::Verify()
{
	//ushort crc = Sys.Crc16((byte*)this, 6);
	//if(Length > 0) crc = Sys.Crc16(Data, Length);

	return Checksum == Crc16;
}

// 构造控制器
Controller::Controller(ITransport* port)
{
	assert_ptr(port);

	// 注册收到数据事件
	port->Register(OnReceive, this);

	_port = port;
}

Controller::~Controller()
{
	int count = _Handlers.Count();
	for(int i=0; i<count; i++)
	{
		if(_Handlers[i]) delete _Handlers[i];
	}
}

uint Controller::OnReceive(ITransport* transport, byte* buf, uint len, void* param)
{
	assert_ptr(param);

	Controller* control = (Controller*)param;
	control->Process(buf, len);

	return 0;
}

void Controller::Process(byte* buf, uint len)
{
	// 只处理本机消息或广播消息。快速处理，高效。
	if(buf[0] != Address && buf[0] != 0) return;

	Message msg;
	if(!msg.Parse(buf, len) || !msg.Verify()) return;

	// 只处理本机消息或广播消息
	//if(msg.Dest != Address && msg.Dest != 0) return;
	// 广播的响应消息也不要
	if(msg.Dest == 0 && msg.Reply) return;

	int count = _Handlers.Count();
	for(int i=0; i<count; i++)
	{
		CommandHandlerLookup* lookup = _Handlers[i];
		if(lookup && lookup->Code == msg.Code)
		{
			// 返回值决定是普通回复还是错误回复
			bool rs = lookup->Handler(msg);
			// 如果本来就是响应，不用回复
			if(!msg.Reply)
			{
				if(rs)
					Reply(msg);
				else
					Error(msg);
			}
			break;
		}
	}
}

void Controller::Register(byte code, CommandHandler handler)
{
	assert_param(code);
	assert_param(handler);

	CommandHandlerLookup* lookup = new CommandHandlerLookup();
	lookup->Code = code;
	lookup->Handler = handler;

	_Handlers.Add(lookup);
}

uint Controller::Send(byte dest, byte code, byte* buf, uint len)
{
	Message msg;
	msg.Init();
	msg.Dest = dest;
	msg.Code = code;
	msg.Length = len;
	msg.Data = buf;

	return Send(msg);
}

bool Controller::Send(Message& msg)
{
	// 附上自己的地址
	msg.Src = Address;

	// 计算校验
	byte* buf = (byte*)&msg;
	ushort crc = Sys.Crc16(buf, 4);
	if(msg.Length > 0)
		crc = Sys.Crc16(msg.Data, msg.Length, crc);

	msg.Checksum = crc;

	// 先发送头部，然后发送负载数据，最后校验
	bool rs = _port->Write(buf, 4);
	if(rs && msg.Length > 0)
		_port->Write(msg.Data, msg.Length);
	_port->Write((byte*)&msg.Checksum, 2);

	return rs;
}

bool Controller::SendSync(Message& msg, uint msTimeout)
{
	if(!Send(msg)) return false;

	// 等待响应
	TimeWheel tw(0, msTimeout, 0);
	while(true)
	{
		if(tw.Expired()) return false;

		// 未完成
		break;
	}

	return true;
}

bool Controller::Reply(Message& msg)
{
	// 回复信息，源地址变成目的地址
	msg.Dest = msg.Src;
	msg.Reply = 1;

	return Send(msg);
}

bool Controller::Error(Message& msg)
{
	msg.Error = 1;

	return Reply(msg);
}

// 常用系统级消息
// 系统时间获取与设置
bool Controller::SysTime(Message& msg)
{
	// 忽略响应消息
	if(msg.Reply) return true;

	debug_printf("Message_SysTime Flags=%d\r\n", msg.Flags);

	// 标识位最低位决定是读时间还是写时间
	if(msg.Flags == 1)
	{
		// 写时间
		if(msg.Length < 8) return false;

		ulong us = *(ulong*)msg.Data;

		Time.SetTime(us);
	}

	// 读时间
	SystemTime& st = Time.Now();
	ulong us2 = st.TotalMicroseconds();
	msg.Length = 8;
	msg.Data = (byte*)&us2;

	return true;
}

bool Controller::SysID(Message& msg)
{
	// 忽略响应消息
	if(msg.Reply) return true;

	debug_printf("Message_SysID Flags=%d\r\n", msg.Flags);

	if(msg.Flags == 0)
	{
		// 12字节ID，4字节CPUID，4字节设备ID
		msg.Length = 5 << 2;
		msg.Data = (byte*)Sys.ID;
	}
	else
	{
		// 干脆直接输出Sys，前面11个uint
		msg.Length = 11 << 2;
		msg.Data = (byte*)&Sys;
	}

	return true;
}

bool Controller_Test_Discover(Message& msg)
{
	if(!msg.Reply)
	{
		debug_printf("收到来自[%d]的Discover请求！", msg.Src);
		if(msg.Length > 0)
		{
			debug_printf("数据：[%d] ", msg.Length);
			Sys.ShowString(msg.Data, msg.Length);
		}
		debug_printf("\r\n");
	}
	else if(!msg.Error)
	{
		debug_printf("收到来自[%d]的Discover响应！", msg.Src);
		if(msg.Length > 0)
		{
			debug_printf("数据：[%d] ", msg.Length);
			Sys.ShowString(msg.Data, msg.Length);
		}
		debug_printf("\r\n");
	}
	else
	{
		debug_printf("收到来自[%d]的Discover错误！", msg.Src);
		if(msg.Length > 0)
		{
			debug_printf("数据：[%d] ", msg.Length);
			Sys.ShowString(msg.Data, msg.Length);
		}
		debug_printf("\r\n");
	}

	return true;
}

void Controller::Test(ITransport* port)
{
	Controller control(port);
	control.Address = 2;

	control.Register(1, SysID);
	control.Register(2, SysTime);

	const byte DISC_CODE = 1;
	control.Register(DISC_CODE, Controller_Test_Discover);

	control.Send(3, DISC_CODE);
}
