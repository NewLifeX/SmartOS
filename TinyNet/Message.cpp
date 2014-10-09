#include "Message.h"

// 初始化消息，各字段为0
void Message::Init()
{
	// 只有POD类型才可以这样清除
	//memset(this, 0, sizeof(Message));
	*(ulong*)&Dest = 0;
	Crc16 = 0;
	Data = NULL;
}

// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
bool Message::Parse(MemoryStream& ms)
{
	byte* buf = ms.Current();
	assert_ptr(buf);

	uint len = ms.Remain();
	assert_param(len > 0);

	// 消息至少4个头部字节、2字节长度和2字节校验，没有负载数据的情况下
	//const int headerSize = 4 + 2 + 2;
	const int headerSize = MESSAGE_SIZE;
	if(len < headerSize) return false;

	// 复制头8字节
	//*(ulong*)this = *(ulong*)buf;
	*(ulong*)&Dest = ms.Read<ulong>();
	// 代码为0是非法的
	if(!Code) return false;
	// 没有源地址是很不负责任的
	if(!Src) return false;
	// 非广播包时，源地址和目的地址相同也是非法的
	if(Dest == Src) return false;

	// 校验剩余长度
	if(len < headerSize + Length) return false;

	// Checksum先清零再计算Crc16
	//ms.Seek(-2);
	ushort* p = (ushort*)ms.Current() - 1;
	//ushort* p = (ushort*)(buf + headerSize - 2);
	*p = 0;
	Crc16 = Sys.Crc16(buf, headerSize + Length);
	*p = Checksum;

	if(Length > 0)
	{
		//Data = buf + headerSize;
		// 要移动游标
		Data = ms.ReadBytes(Length);
	}

	return true;
}

// 验证消息校验和是否有效
bool Message::Verify()
{
	//ushort crc = Sys.Crc16((byte*)this, 6);
	//if(Length > 0) crc = Sys.Crc16(Data, Length);

	return Checksum == Crc16;
}

void Message::Write(MemoryStream& ms)
{
	ms.Write(*(ulong*)&Dest);
	if(Length > 0) ms.Write(Data, 0, Length);
}

// 构造控制器
Controller::Controller(ITransport* port)
{
	assert_ptr(port);

	debug_printf("微网控制器初始化 传输口：%s\r\n", port->ToString());

	// 注册收到数据事件
	port->Register(OnReceive, this);

	_ports = &port;
	_portCount = 1;

	Init();
}

Controller::Controller(ITransport* ports[], int count)
{
	assert_ptr(ports);
	assert_param(count > 0);

	// 内部分配空间存放，避免外部内存被回收
	_ports = new ITransport*[count];

	debug_printf("微网控制器初始化 共%d个传输口", count);
	for(int i=0; i<count; i++)
	{
		assert_ptr(ports[i]);

		debug_printf(" %s", ports[i]->ToString());
		_ports[i] = ports[i];
	}
	debug_printf("\r\n");

	// 注册收到数据事件
	for(int i=0; i<count; i++)
	{
		ports[i]->Register(OnReceive, this);
	}

	_portCount = count;

	Init();
}

void Controller::Init()
{
	_Sequence = 0;
	_Timer = NULL;

	memset(_Handlers, 0, ArrayLength(_Handlers));
	_HandlerCount = 0;

	// 初始化一个随机地址
	Address = Sys.ID[0];
	// 如果地址为0，则使用时间来随机一个
	while(!Address) Address = Time.Current();

	debug_printf("初始化微网控制器完成 Address=%d (0x%02x) 使用%d个传输接口\r\n", Address, Address, _portCount);
}

Controller::~Controller()
{
	for(int i=0; i<_HandlerCount; i++)
	{
		if(_Handlers[i]) delete _Handlers[i];
	}

	if(_ports) delete _ports;
	_ports = NULL;

	if(_Timer) delete _Timer;
	_Timer = NULL;

	foreach(QueueNode*, node, _Queue)
	{
		delete node;
		foreach_remove();
	}
	foreach_end();

	debug_printf("微网控制器销毁\r\n");
}

uint Controller::OnReceive(ITransport* transport, byte* buf, uint len, void* param)
{
	assert_ptr(param);

	Controller* control = (Controller*)param;

	// 这里使用数据流，可能多个消息粘包在一起
	MemoryStream ms(buf, len);
	while(ms.Remain() >= MESSAGE_SIZE)
	{
		// 如果不是有效数据包，则直接退出，避免产生死循环。当然，也可以逐字节移动测试，不过那样性能太差
		if(!control->Process(ms, transport)) break;
	}

	return 0;
}

bool Controller::Process(MemoryStream& ms, ITransport* port)
{
#if DEBUG
	byte* p = ms.Current();
#endif

	// 只处理本机消息或广播消息。快速处理，高效。
	byte dest = ms.Peek();
	if(dest != Address && dest != 0) return false;

	Message msg;
	if(!msg.Parse(ms)) return false;

#if DEBUG
	assert_param(ms.Current() - p == MESSAGE_SIZE + msg.Length);
	// 输出整条信息
	/*Sys.ShowHex(p, ms.Current() - p);
	debug_printf("\r\n");*/

	debug_printf("%s ", port->ToString());
	if(msg.Error)
		debug_printf("Message Error");
	else if(msg.Reply)
		debug_printf("Message Reply");
	else
		debug_printf("Message Request");

	debug_printf(" %d => %d Code=%d Length=%d Checksum=0x%04x", msg.Src, msg.Dest, msg.Code, msg.Length, msg.Checksum);
	if(msg.Length > 0)
	{
		debug_printf(" 数据：[%d] ", msg.Length);
		Sys.ShowString(msg.Data, msg.Length);
	}
	if(!msg.Verify()) debug_printf(" Crc Error 0x%04x", msg.Crc16);
	debug_printf("\r\n");
#endif

	// 广播的响应消息也不要
	if(msg.Dest == 0 && msg.Reply) return true;

	// 校验
	if(!msg.Verify())
	{
		// 错误的响应包直接忽略
		if(msg.Reply) return true;

		// 不需要确认的也不费事了
		if(!msg.Confirm) return true;
		
#if DEBUG
		byte err[] = "Crc Error #XXXX";
		uint len = ArrayLength(err);
		// 把Crc16附到后面4字节
		Sys.ToHex(err + len - 5, (byte*)&msg.Crc16, 2);

		msg.Length = len;
		msg.Data = err;
#else
		msg.Length = 0;
#endif

		Error(msg, port);

		return true;
	}

	// 如果是响应消息，及时更新请求队列
	if(msg.Reply)
	{
		foreach(QueueNode*, node, _Queue)
		{
			if(node->Port == port && node->Msg->Sequence == msg.Sequence)
			{
				delete node;
				foreach_remove();
				break;
			}
		}
		foreach_end();
	}
	
	// 选择处理器来处理消息
	for(int i=0; i<_HandlerCount; i++)
	{
		CommandHandlerLookup* lookup = _Handlers[i];
		if(lookup && lookup->Code == msg.Code)
		{
			// 返回值决定是普通回复还是错误回复
			bool rs = lookup->Handler(msg, lookup->Param);
			// 如果本来就是响应，不用回复
			if(!msg.Reply && msg.Confirm)
			{
				if(rs)
					Reply(msg, port);
				else
					Error(msg, port);
			}
			break;
		}
	}

	return true;
}

void Controller::Register(byte code, CommandHandler handler, void* param)
{
	assert_param(code);
	assert_param(handler);

	CommandHandlerLookup* lookup;

#if DEBUG
	// 检查是否已注册。一般指令码是固定的，所以只在DEBUG版本检查
	for(int i=0; i<_HandlerCount; i++)
	{
		lookup = _Handlers[i];
		if(lookup && lookup->Code == code)
		{
			debug_printf("Controller::Register Error! Code=%d was Registered to 0x%08x\r\n", code, lookup->Handler);
			return;
		}
	}
#endif

	lookup = new CommandHandlerLookup();
	lookup->Code = code;
	lookup->Handler = handler;
	lookup->Param = param;

	_Handlers[_HandlerCount++] = lookup;
}

uint Controller::Send(byte dest, byte code, byte* buf, uint len, ITransport* port)
{
	Message msg;
	msg.Init();
	msg.Dest = dest;
	msg.Code = code;
	msg.Length = len;
	msg.Data = buf;

	return Send(msg, port);
}

bool Controller::Send(Message& msg, ITransport* port, uint msTimeout)
{
	// 附上自己的地址
	msg.Src = Address;

	// 是否需要响应
	msg.Confirm = !msg.Reply && msTimeout > 0 ? 1 : 0;
	
	// 附上序列号
	msg.Sequence = ++_Sequence;

	// 指针直接访问消息头。如果没有负载数据，它就是全部
	byte* buf = (byte*)&msg.Dest;
	//uint len = msg.Length;

	// 计算校验，先清零
	msg.Checksum = 0;
	ushort crc = Sys.Crc16(buf, MESSAGE_SIZE);
	if(msg.Length > 0)
		crc = Sys.Crc16(msg.Data, msg.Length, crc);

	msg.Checksum = msg.Crc16 = crc;

#if DEBUG
	debug_printf("Message Send");
	if(msg.Error)
		debug_printf(" Error");
	else if(msg.Reply)
		debug_printf(" Reply");

	debug_printf(" %d => %d Code=%d Length=%d Checksum=0x%04x", msg.Src, msg.Dest, msg.Code, msg.Length, msg.Checksum);
	if(msg.Length > 0)
	{
		debug_printf(" 数据：[%d] ", msg.Length);
		Sys.ShowString(msg.Data, msg.Length);
	}
	if(!msg.Verify()) debug_printf(" Crc Error 0x%04x", msg.Crc16);
	debug_printf("\r\n");

	/*Sys.ShowHex(buf, MESSAGE_SIZE);
	if(msg.Length > 0)
		Sys.ShowHex(msg.Data, msg.Length);
	debug_printf("\r\n");*/
#endif

	if(msg.Reply || !msTimeout) return SendInternal(msg, port);

	// 非响应消息，考虑异步

	QueueNode* first = new QueueNode();
	first->Expired	= Time.Current() + msTimeout * 1000;
	first->Msg	= &msg;
	first->Port	= port;

	// 消息队列的设计，没有考虑多线程冲突，可能会有问题
	_Queue.Add(first);

	// 发往所有端口
	if(!port)
	{
		first->Port = _ports[0];
		for(int i=1; i<_portCount; i++)
		{
			QueueNode* node = new QueueNode(*first);
			node->Port = _ports[i];

			_Queue.Add(node);
		}
	}
	// 准备增加一个定时任务，用于轮询重发队列里面的任务
	if(!_Timer)
	{
		// 抽取一个空闲定时器，用于轮询重发队列
		_Timer = Timer::Create();
		_Timer->Register(SendTask, this);
		_Timer->SetFrequency(100);
	}
	// 先发送一次，再开启定时器轮询
	bool rs = SendInternal(msg, port);
	_Timer->Start();

	return rs;
}

bool Controller::SendInternal(Message& msg, ITransport* port)
{
	// 指针直接访问消息头。如果没有负载数据，它就是全部
	byte* buf = (byte*)&msg.Dest;
	uint len = msg.Length;

	// ms需要在外面这里声明，否则离开大括号作用域以后变量被销毁，导致缓冲区不可用
	MemoryStream ms(MESSAGE_SIZE + msg.Length);
	if(msg.Length > 0)
	{
		// 带有负载数据，需要合并成为一段连续的内存
		msg.Write(ms);
		ms.SetPosition(0);

		buf = ms.Current();
		len = ms.Length;
	}

	bool rs = true;
	if(port)
		rs = port->Write(buf, len);
	else
	{
		// 发往所有端口
		for(int i=0; i<_portCount; i++)
			rs &= _ports[i]->Write(buf, len);
	}

	return rs;
}

void Controller::SendTask(void* sender, void* param)
{
	assert_ptr(param);

	Controller* control = (Controller*)param;
	control->SendTask();
}

void Controller::SendTask()
{
	// 如果没有轮询队列，则关闭定时器
	if(!_Queue.Count())
	{
		if(_Timer) _Timer->Stop();
		return;
	}

	/*auto_ptr< IEnumerator<QueueNode*> > it(_Queue.GetEnumerator());
	while(it->MoveNext())
	{
		QueueNode* node = it->Current();

		// 如果过期，则删除
		if(node->Expired < Time.Current())
		{
			_Queue.Remove(node);
			delete node;
		}
		else
		{
			// 发送消息
			SendInternal(*node->Msg, node->Port);
		}
	}*/
	
	foreach(QueueNode*, node, _Queue)
	{
		// 如果过期，则删除
		if(node->Expired < Time.Current())
		{
			delete node;
			foreach_remove();
		}
		else
		{
			// 发送消息
			SendInternal(*node->Msg, node->Port);
		}
	}
	foreach_end();
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

bool Controller::Reply(Message& msg, ITransport* port)
{
	// 回复信息，源地址变成目的地址
	msg.Dest = msg.Src;
	msg.Reply = 1;

	return Send(msg, port);
}

bool Controller::Error(Message& msg, ITransport* port)
{
	msg.Error = 1;

	return Reply(msg, port);
}

// 常用系统级消息
// 系统时间获取与设置
bool Controller::SysTime(Message& msg, void* param)
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
	ulong us2 = Time.Current();
	msg.Length = 8;
	msg.Data = (byte*)&us2;

	return true;
}

bool Controller::SysID(Message& msg, void* param)
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

bool Controller::Discover(Message& msg, void* param)
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

	const byte DISC_CODE = 3;
	control.Register(DISC_CODE, Discover);

	control.Send(3, DISC_CODE);
}
