#include "Message.h"

// 初始化消息，各字段为0
Message::Message(byte code)
{
	// 只有POD类型才可以这样清除
	//memset(this, 0, sizeof(Message));
	//*(ulong*)&Dest = 0;
	// 如果地址不是8字节对齐，长整型操作会导致异常
	memset(&Dest, 0, 8);
	Code = code;
	Crc16 = 0;
	//Data = NULL;
}

Message::Message(Message& msg)
{
	//*(ulong*)&Dest = *(ulong*)&msg.Dest;
	// 如果地址不是8字节对齐，长整型操作会导致异常
	memcpy(&Dest, &msg.Dest, 8);

	Crc16 = Crc16;
	ArrayCopy(Data, msg.Data);
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

	// 复制头8字节。没有负载数据时，该分析已经完整
	//*(ulong*)&Dest = ms.Read<ulong>();
	// 如果地址不是8字节对齐，长整型操作会导致异常
	memcpy(&Dest, ms.ReadBytes(8), 8);
	// 代码为0是非法的
	if(!Code) return false;
	// 没有源地址是很不负责任的
	if(!Src) return false;
	// 非广播包时，源地址和目的地址相同也是非法的
	if(Dest == Src) return false;

	// 校验剩余长度
	if(len < headerSize + Length) return false;

	// 直接计算Crc16
	Crc16 = Sys.Crc16(buf, headerSize + Length - 2);

	if(Length > 0)
	{
		// 前面错误地把2字节数据当作校验码
		ms.Seek(-2);
		// 要移动游标
		//Data = ms.ReadBytes(Length);
		//memcpy(Data, ms.ReadBytes(Length), Length);
		ms.Read(Data, 0, Length);
		// 读取真正的校验码
		Checksum = ms.Read<ushort>();
	}

	return true;
}

// 验证消息校验和是否有效
bool Message::Verify()
{
	return Checksum == Crc16;
}

void Message::ComputeCrc()
{
	MemoryStream ms(MESSAGE_SIZE + Length);
	byte* buf = ms.Current();

	Write(ms);

	Checksum = Crc16 = Sys.Crc16(buf, ms.Length - 2);
}

// 设置数据。
void Message::SetData(byte* buf, uint len)
{
	//assert_ptr(buf);
	//assert_param(len > 0 && len <= ArrayLength(Data));
	assert_param(len <= ArrayLength(Data));

	Length = len;
	if(len > 0)
	{
		assert_ptr(buf);
		memcpy(Data, buf, len);
	}
}

void Message::Write(MemoryStream& ms)
{
	//ms.Write(*(ulong*)&Dest);
	// 如果地址不是8字节对齐，长整型操作会导致异常
	ms.Write((byte*)&Dest, 0, 8);
	if(Length > 0)
	{
		// 前面错误地把2字节数据当作校验码
		ms.Seek(-2);
		ms.Write(Data, 0, Length);
		// 读取真正的校验码
		ms.Write(Checksum);
	}
}

// 构造控制器
Controller::Controller(ITransport* port)
{
	assert_ptr(port);

	debug_printf("微网控制器初始化 传输口：%s\r\n", port->ToString());

	// 注册收到数据事件
	port->Register(OnReceive, this);

	_ports.Add(port);

	Init();
}

Controller::Controller(ITransport* ports[], int count)
{
	assert_ptr(ports);
	assert_param(count > 0 && count < ArrayLength(_ports));

	debug_printf("微网控制器初始化 共%d个传输口", count);
	for(int i=0; i<count; i++)
	{
		assert_ptr(ports[i]);

		debug_printf(" %s", ports[i]->ToString());
		_ports.Add(ports[i]);
	}
	debug_printf("\r\n");

	// 注册收到数据事件
	for(int i=0; i<count; i++)
	{
		ports[i]->Register(OnReceive, this);
	}

	Init();
}

void Controller::Init()
{
	_Sequence = 0;

	ArrayZero(_Handlers);
	_HandlerCount = 0;

	// 初始化一个随机地址
	Address = Sys.ID[0];
	// 如果地址为0，则使用时间来随机一个
	while(!Address) Address = Time.Current();

	debug_printf("初始化微网控制器完成 Address=%d (0x%02x) 使用%d个传输接口\r\n", Address, Address, _ports.Count());
}

Controller::~Controller()
{
	for(int i=0; i<_HandlerCount; i++)
	{
		if(_Handlers[i]) delete _Handlers[i];
	}

	_ports.DeleteAll().Clear();

	debug_printf("微网控制器销毁\r\n");
}

void Controller::AddTransport(ITransport* port)
{
	assert_ptr(port);

	//_ports[_portCount++] = port;
	_ports.Add(port);
}

uint Controller::OnReceive(ITransport* transport, byte* buf, uint len, void* param)
{
	assert_ptr(param);

	Controller* control = (Controller*)param;

	// 这里使用数据流，可能多个消息粘包在一起
	// 注意，此时指针位于0，而内容长度为缓冲区长度
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
	// 输出整条信息
	/*Sys.ShowHex(p, ms.Length, '-');
	debug_printf("\r\n");*/
#endif

	// 只处理本机消息或广播消息。快速处理，高效。
	byte dest = ms.Peek();
	if(dest != Address && dest != 0) return false;

	Message msg;
	if(!msg.Parse(ms)) return false;

	// 处理重复消息
	if(_Ring.Find(msg.Sequence) >= 0) return false;
	_Ring.Push(msg.Sequence);

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
		Sys.ShowString(msg.Data, msg.Length, false);
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

		//msg.Length = len;
		//msg.Data = err;
		msg.SetData(err, len);
#else
		msg.Length = 0;
#endif

		Error(msg, port);

		return true;
	}

	// 如果是响应消息，及时更新请求队列
	if(msg.Reply)
	{
		int i = -1;
		while(_Queue.MoveNext(i))
		{
			MessageQueue* node = _Queue[i];
			if(node->Msg->Sequence == msg.Sequence)
			{
				// 该传输口收到响应，从就绪队列中删除
				node->Ports.Remove(port);

				// 复制一份响应消息
				node->Reply = new Message(msg);
			}
		}
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
	Message msg(code);
	msg.Dest = dest;
	msg.SetData(buf, len);

	return Send(msg, port);
}

void Controller::PrepareSend(Message& msg)
{
	// 附上自己的地址
	msg.Src = Address;

	// 附上序列号。响应消息保持序列号不变
	if(!msg.Reply) msg.Sequence = ++_Sequence;

	// 计算校验
	msg.ComputeCrc();

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
		Sys.ShowString(msg.Data, msg.Length, false);
	}
	if(!msg.Verify()) debug_printf(" Crc Error 0x%04x", msg.Crc16);
	debug_printf("\r\n");

	/*Sys.ShowHex(buf, MESSAGE_SIZE);
	if(msg.Length > 0)
		Sys.ShowHex(msg.Data, msg.Length);
	debug_printf("\r\n");*/
#endif
}

bool Controller::Send(Message& msg, ITransport* port)
{
	// 是否需要响应
	//msg.Confirm = !msg.Reply && msTimeout > 0 ? 1 : 0;
	// 不去修正Confirm，由外部决定好了

	PrepareSend(msg);

	// 指针直接访问消息头。如果没有负载数据，它就是全部
	byte* buf = (byte*)&msg.Dest;
	uint len = msg.Length;

	// ms需要在外面这里声明，否则离开大括号作用域以后变量被销毁，导致缓冲区不可用
	MemoryStream ms(MESSAGE_SIZE + msg.Length);
	if(msg.Length > 0)
	{
		buf = ms.Current();
		len = ms.Length;

		// 带有负载数据，需要合并成为一段连续的内存
		msg.Write(ms);
	}

	bool rs = true;
	if(port)
		rs = port->Write(buf, len);
	else
	{
		// 发往所有端口
		int i = -1;
		while(_ports.MoveNext(i))
			rs &= _ports[i]->Write(buf, len);
	}

	return rs;
}

bool Controller::SendSync(Message& msg, uint msTimeout)
{
	// 需要响应
	msg.Confirm = true;

	PrepareSend(msg);

	// 准备消息队列
	MessageQueue queue;
	queue.SetMessage(msg);
	queue.Ports = _ports;

	// 加入等待队列
	_Queue.Add(&queue);

	// 等待响应
	bool rs = false;
	TimeWheel tw(0, msTimeout, 0);
	while(!tw.Expired())
	{
		// 发送消息
		int i = -1;
		while(queue.Ports.MoveNext(i))
		{
			queue.Ports[i]->Write(queue.Data, queue.Length);
		}

		// 等一会
		Sys.Sleep(50);

		// 检查未完成传输口
		if(queue.Ports.Count() == 0) { rs = true; break; }
	}

	_Queue.Remove(&queue);

	return rs;
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

	debug_printf("Message_SysTime Length=%d\r\n", msg.Length);

	// 负载数据决定是读时间还是写时间
	if(msg.Length >= 8)
	{
		// 写时间
		ulong us = *(ulong*)msg.Data;

		Time.SetTime(us);
	}

	// 读时间
	ulong us2 = Time.Current();
	msg.Length = 8;
	*(ulong*)msg.Data = us2;

	return true;
}

bool Controller::SysID(Message& msg, void* param)
{
	// 忽略响应消息
	if(msg.Reply) return true;

	debug_printf("Message_SysID Length=%d\r\n", msg.Length);

	if(msg.Length == 0)
	{
		// 12字节ID，4字节CPUID，4字节设备ID
		msg.SetData(Sys.ID, 5 << 2);
	}
	else
	{
		// 干脆直接输出Sys，前面11个uint
		msg.SetData((byte*)&Sys, 11 << 2);
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
			Sys.ShowString(msg.Data, msg.Length, false);
		}
		debug_printf("\r\n");
	}
	else if(!msg.Error)
	{
		debug_printf("收到来自[%d]的Discover响应！", msg.Src);
		if(msg.Length > 0)
		{
			debug_printf("数据：[%d] ", msg.Length);
			Sys.ShowString(msg.Data, msg.Length, false);
		}
		debug_printf("\r\n");
	}
	else
	{
		debug_printf("收到来自[%d]的Discover错误！", msg.Src);
		if(msg.Length > 0)
		{
			debug_printf("数据：[%d] ", msg.Length);
			Sys.ShowString(msg.Data, msg.Length, false);
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

MessageQueue::MessageQueue()
{
	Reply = NULL;
}

MessageQueue::~MessageQueue()
{
	delete Reply;
	Reply = NULL;
}

void MessageQueue::SetMessage(Message& msg)
{
	Msg = &msg;

	byte* buf = (byte*)&msg.Dest;
	if(!msg.Length)
	{
		Length = MESSAGE_SIZE;
		memcpy(Data, buf, MESSAGE_SIZE);
	}
	else
	{
		// 注意，此时指针位于0，而内容长度为缓冲区长度
		MemoryStream ms(Data, ArrayLength(Data));
		ms.Length = 0;
		msg.Write(ms);
		Length = ms.Length;
	}
}

RingQueue::RingQueue()
{
	Index = 0;
	ArrayZero(Arr);
}

void RingQueue::Push(byte item)
{
	Arr[Index++] = item;
	if(Index == ArrayLength(Arr)) Index = 0;
}

int RingQueue::Find(byte item)
{
	for(int i=0; i < ArrayLength(Arr); i++)
	{
		if(Arr[i] == item) return i;
	}

	return -1;
}
