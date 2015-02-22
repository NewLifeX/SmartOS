#include "Controller.h"

#define MSG_DEBUG DEBUG
//#define MSG_DEBUG 0
#if MSG_DEBUG
	#define msg_printf debug_printf
#else
	__inline void msg_printf( const char *format, ... ) {}
#endif

// 构造控制器
Controller::Controller(ITransport* port)
{
	assert_ptr(port);

	debug_printf("\r\nTinyNet::Init 传输口：%s\r\n", port->ToString());

	// 注册收到数据事件
	port->Register(Dispatch, this);

	_ports.Add(port);

	Init();

	//_ports[_portCount++] = port;
}

Controller::Controller(ITransport* ports[], int count)
{
	assert_ptr(ports);
	assert_param(count > 0 && count < ArrayLength(_ports));

	debug_printf("\r\nTinyNet::Init 共[%d]个传输口", count);
	for(int i=0; i<count && i<4; i++)
	{
		assert_ptr(ports[i]);

		debug_printf(" %s", ports[i]->ToString());
		//_ports[_portCount++] = ports[i];
		_ports.Add(ports[i]);
	}
	debug_printf("\r\n");

	// 注册收到数据事件
	for(int i=0; i<count; i++)
	{
		ports[i]->Register(Dispatch, this);
	}

	Init();
}

void Controller::Init()
{
	MinSize = 0;

	//ArrayZero(_ports);
	//_portCount = 0;

	//ArrayZero(_Handlers);
	//_HandlerCount = 0;

	// 初始化一个随机地址
	Address = Sys.ID[0];
	// 如果地址为0，则使用时间来随机一个
	while(!Address) Address = Time.Current();

	debug_printf("TinyNet::Inited Address=%d (0x%02x) 使用[%d]个传输接口\r\n", Address, Address, _ports.Count());

	Received	= NULL;
	Param		= NULL;
}

Controller::~Controller()
{
	_Handlers.DeleteAll().Clear();
	/*for(int i=0; i<_HandlerCount; i++)
	{
		if(_Handlers[i]) delete _Handlers[i];
	}*/

	_ports.DeleteAll().Clear();
	/*for(int i=0; i<_ports.Count(); i++)
	{
		delete _ports[i];
		//_ports[i] = NULL;
	}
	_ports.Clear();*/

	debug_printf("TinyNet::UnInit\r\n");
}

// 添加传输口
void Controller::AddTransport(ITransport* port)
{
	assert_ptr(port);
	assert_param(_ports.Count() < 4);

	debug_printf("\r\nTinyNet::AddTransport 添加传输口：%s\r\n", port->ToString());

	// 注册收到数据事件
	port->Register(Dispatch, this);

	_ports.Add(port);
}

uint Controller::Dispatch(ITransport* transport, byte* buf, uint len, void* param)
{
	assert_ptr(buf);
	assert_ptr(param);

	Controller* control = (Controller*)param;

	// 这里使用数据流，可能多个消息粘包在一起
	// 注意，此时指针位于0，而内容长度为缓冲区长度
	MemoryStream ms(buf, len);
	while(ms.Remain() >= control->MinSize)
	{
		// 如果不是有效数据包，则直接退出，避免产生死循环。当然，也可以逐字节移动测试，不过那样性能太差
		if(!control->Dispatch(ms, transport)) break;
	}

	return 0;
}

bool Controller::Dispatch(MemoryStream& ms, ITransport* port)
{
	byte* buf = ms.Current();

#if MSG_DEBUG
	/*msg_printf("TinyNet::Dispatch ");
	// 输出整条信息
	Sys.ShowHex(buf, ms.Length, '-');
	msg_printf("\r\n");*/
#endif

	auto_ptr<Message> p_msg(Create());
	Message& msg = *p_msg;
	if(!msg.Read(ms)) return false;

	// 校验
	if(!msg.Valid()) return true;

	if(!Valid(msg, port)) return true;

	// 选择处理器来处理消息
	//for(int i=0; i<_HandlerCount; i++)
	// 倒序选择处理器来处理消息，让后注册处理器有更高优先权
	for(int i = _Handlers.Count() - 1; i >= 0; i--)
	{
		HandlerLookup* lookup = _Handlers[i];
		if(lookup && lookup->Code == msg.Code)
		{
			// 返回值决定是否成功处理，如果未成功处理，则技术遍历下一个处理器
			bool rs = lookup->Handler(msg, lookup->Param);
			if(rs) break;
		}
	}

	// 外部公共消息事件
	if(Received)
	{
		if(!Received(msg, Param)) return true;
	}

	return true;
}

bool Controller::Valid(Message& msg, ITransport* port)
{
	return true;
}

void Controller::Register(byte code, MessageHandler handler, void* param)
{
	assert_param(code);
	assert_param(handler);

	HandlerLookup* lookup;

#if DEBUG
	// 检查是否已注册。一般指令码是固定的，所以只在DEBUG版本检查
	for(int i=0; i<_Handlers.Count(); i++)
	{
		lookup = _Handlers[i];
		if(lookup && lookup->Code == code)
		{
			debug_printf("Controller::Register Error! Code=%d was Registered to 0x%08x\r\n", code, lookup->Handler);
			//return;
		}
	}
#endif

	lookup = new HandlerLookup();
	lookup->Code = code;
	lookup->Handler = handler;
	lookup->Param = param;

	//_Handlers[_HandlerCount++] = lookup;
	_Handlers.Add(lookup);
}

int Controller::Send(Message& msg, ITransport* port)
{
	// 如果没有传输口处于打开状态，则发送失败
	if(port && !port->Open()) return -1;
	bool rs = false;
	//int i = -1;
	//while(_ports.MoveNext(i))
	//	rs |= _ports[i]->Open();
	for(int i=0; i<_ports.Count(); i++)
		rs |= _ports[i]->Open();
	if(!rs) return -1;

	uint len = msg.Size();

	// ms需要在外面这里声明，否则离开大括号作用域以后变量被销毁，导致缓冲区不可用
	MemoryStream ms(len);
	// 带有负载数据，需要合并成为一段连续的内存
	msg.Write(ms);
	assert_param(len == ms.Length);
	// 内存流扩容以后，指针会改变
	byte* buf = ms.GetBuffer();

	//Sys.ShowHex(buf, len, '-');
	//msg_printf("\r\n");
	if(port)
	{
		rs = port->Write(buf, len);
		return rs ? 1 : 0;
	}

	int count = 0;
	// 发往所有端口
	//i = -1;
	//while(_ports.MoveNext(i))
	for(int i=0; i<_ports.Count(); i++)
	{
		if(_ports[i]->Write(buf, len)) count++;
	}

	return count;
}

int Controller::Reply(Message& msg, ITransport* port)
{
	msg.Reply = 1;

	return Send(msg, port);
}
