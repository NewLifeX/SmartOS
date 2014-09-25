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
	// 复制校验和
	Checksum = *(ushort*)(buf + 4);

	if(len > headerSize)
	{
		Length = len - headerSize;
		Data = buf + headerSize;
	}
	else
	{
		Length = 0;
		Data = NULL;
	}

	return true;
}

// 验证消息校验和是否有效
bool Message::Verify()
{
	return false;
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
	Message msg;
	if(!msg.Parse(buf, len) || !msg.Verify()) return;

	int count = _Handlers.Count();
	for(int i=0; i<count; i++)
	{
		CommandHandlerLookup* lookup = _Handlers[i];
		if(lookup && lookup->Code == msg.Code)
		{
			// 如果处理成功则退出，否则继续遍历其它处理器
			if(lookup->Handler(msg)) return;
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

bool Controller::Send(Message& msg, uint msTimeout)
{
	// 附上自己的地址
	msg.Src = Address;

	// 先发送头部，然后发送负载数据
	byte* buf = (byte*)&msg;
	_port->Write(buf, 6);
	if(msg.Length > 0)
		_port->Write(msg.Data, msg.Length);

	/*// 等待响应
	TimeWheel tw(0, msTimeout, 0);
	while(true)
	{
		if(tw.Expired) return false;

		xxx
	}*/

	return true;
}

void Controller::Reply(Message& msg)
{
	msg.Reply = 1;
	Send(msg);
}

bool Controller_Test_Discover(const Message& msg)
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

	const byte DISC_CODE = 1;
	control.Register(DISC_CODE, Controller_Test_Discover);

	control.Send(3, DISC_CODE);
}
