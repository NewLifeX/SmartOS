#include "TinyMessage.h"

#define MSG_DEBUG DEBUG
//#define MSG_DEBUG 0
#if MSG_DEBUG
	#define msg_printf debug_printf
#else
	__inline void msg_printf( const char *format, ... ) {}
#endif

void SendTask(void* param);
void StatTask(void* param);

// 初始化消息，各字段为0
TinyMessage::TinyMessage(byte code) : Message(code)
{
	Data = _Data;

	// 如果地址不是8字节对齐，长整型操作会导致异常
	memset(&Dest, 0, MinSize);

	Crc = 0;
	TTL = 0;
#if MSG_DEBUG
	Retry = 1;
#endif
}

TinyMessage::TinyMessage(TinyMessage& msg) : Message(msg)
{
	Data = _Data;

	memcpy(&Dest, &msg.Dest, MinSize);

	Crc = msg.Crc;

	assert_ptr(Data);
	assert_ptr(msg.Data);

	if(Length) memcpy(Data, msg.Data, Length);
	TTL = msg.TTL;
#if MSG_DEBUG
	Retry = msg.Retry;
#endif
}

// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
bool TinyMessage::Read(Stream& ms)
{
	// 消息至少4个头部字节、2字节长度和2字节校验，没有负载数据的情况下
	if(ms.Remain() < MinSize) return false;

	ms.Read(&Dest, 0, HeaderSize);
	// 占位符拷贝到实际数据
	Code	= _Code;
	Length	= _Length;
	Reply	= _Reply;

	// 代码为0是非法的
	if(!Code) return false;
	// 没有源地址是很不负责任的
	if(!Src) return false;
	// 非广播包时，源地址和目的地址相同也是非法的
	if(Dest == Src) return false;

	// 校验剩余长度
	if(ms.Remain() < Length + 2) return false;

	if(Length > 0) ms.Read(Data, 0, Length);

	// 读取真正的校验码
	Checksum = ms.Read<ushort>();

	// 连续的，可以直接计算Crc16
	Crc = Sys.Crc16(&Dest, HeaderSize + Length);

	// 后面可能有TTL
	if(UseTTL)
	{
		// 必须严格检查，否则可能成为溢出漏洞
		if(ms.Remain() > 0)
			TTL = ms.Read<byte>();
		else
			TTL = 0;
	}
#if MSG_DEBUG
	// 调试诊断模式下该字段表示第几次重发
	if(ms.Remain() > 0)
		Retry = ms.Read<byte>();
	else
		Retry = 0;
#endif

	return true;
}

void TinyMessage::Write(Stream& ms)
{
	// 实际数据拷贝到占位符
	_Code	= Code;
	_Length	= Length;
	_Reply	= Reply;

	byte* buf = ms.Current();
	// 不要写入验证码
	ms.Write((byte*)&Dest, 0, HeaderSize);
	if(Length > 0) ms.Write(Data, 0, Length);

	Checksum = Crc = Sys.Crc16(buf, HeaderSize + Length);
	// 写入真正的校验码
	ms.Write(Checksum);

	// 后面可能有TTL
	if(UseTTL) ms.Write(TTL);
#if MSG_DEBUG
	ms.Write(Retry);
#endif
}

void TinyMessage::ComputeCrc()
{
	Stream ms(Size());

	Write(ms);

	// 扣除不计算校验码的部分
	Checksum = Crc = Sys.Crc16(ms.GetBuffer(), HeaderSize + Length);
}

// 验证消息校验码是否有效
bool TinyMessage::Valid() const
{
	if(Checksum == Crc) return true;

	debug_printf("Message::Valid Crc Error %04X != Checksum: %04X \r\n", Crc, Checksum);
#if MSG_DEBUG
	debug_printf("校验错误指令 ");
	Show();
#endif

	return false;
}

// 消息所占据的指令数据大小。包括头部、负载数据和附加数据
uint TinyMessage::Size() const
{
	uint len = MinSize + Length;
	if(UseTTL) len++;
#if MSG_DEBUG
	len++;
#endif
	return len;
}

void TinyMessage::Show() const
{
#if MSG_DEBUG
	assert_ptr(this);
	msg_printf("0x%02X => 0x%02X Code=0x%02X Flag=0x%02X Seq=%d Length=%d Checksum=0x%04x Retry=%d", Src, Dest, Code, *((byte*)&_Code+1), Sequence, Length, Checksum, Retry);
	if(Length > 0)
	{
		assert_ptr(Data);
		msg_printf(" Data[%d]=", Length);
		//Sys.ShowString(Data, Length, false);
		ByteArray bs(Data, Length);
		bs.Show();
	}
	if(Checksum != Crc) msg_printf(" Crc Error 0x%04x [%04X]", Crc, __REV16(Crc));
	msg_printf("\r\n");
#endif
}

// 构造控制器
TinyController::TinyController() : Controller()
{
	_Sequence	= 0;
	_taskID		= 0;
	Interval	= 8000;
	Timeout		= 50000;

	MinSize = TinyMessage::MinSize;

	// 初始化一个随机地址
	Address = Sys.ID[0];
	// 如果地址为0，则使用时间来随机一个
	//while(!Address) Address = Time.Current();
	// 节点地址范围2~254，网关专用0x01，节点让步
	while(Address < 2 || Address > 254)
	{
		Sys.Delay(30);
		Address = Time.Current();
	}

	ArrayZero(_Queue);
}

TinyController::~TinyController()
{
	if(_taskID) Sys.RemoveTask(_taskID);
}

void TinyController::Open()
{
	if(Opened) return;

	debug_printf("TinyNet::Inited Address=%d (0x%02x) 使用传输接口 %s\r\n", Address, Address, Port->ToString());

	Controller::Open();

	if(!_taskID)
	{
		debug_printf("TinyNet::微网消息队列 ");
		_taskID = Sys.AddTask(SendTask, this, 0, 10000);
		// 默认禁用，有数据要发送才开启
		Sys.SetTask(_taskID, false);
	}

	memset(&Total, 0, sizeof(Total));
	memset(&Last, 0, sizeof(Last));

	// 因为统计不准确，暂时不显示状态统计
	debug_printf("TinyNet::统计 ");
	Sys.AddTask(StatTask, this, 1000000, 5000000);
}

void ShowMessage(TinyMessage& msg, bool send, ITransport* port)
{
	if(msg.Ack) return;

	if(send)
		msg_printf("TinyMessage::Send ");
	else
	{
		msg_printf("%s", port->ToString());
		msg_printf("::");
	}
	if(msg.Error)
		msg_printf("Error ");
	else if(msg.Ack)
		msg_printf("Ack ");
	else if(msg.Reply)
		msg_printf("Reply ");
	else if(!send)
		msg_printf("Request ");

	msg.Show();
}

bool TinyController::Dispatch(Stream& ms, Message* pmsg)
{
	TinyMessage msg;
	return Controller::Dispatch(ms, &msg);
}

// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
bool TinyController::Valid(Message& msg)
{
	TinyMessage& tmsg = (TinyMessage&)msg;

	// 代码为0是非法的
	if(!msg.Code) return false;
	// 没有源地址是很不负责任的
	if(!tmsg.Src) return false;
	// 非广播包时，源地址和目的地址相同也是非法的
	if(tmsg.Dest == tmsg.Src) return false;
	// 只处理本机消息或广播消息。快速处理，高效。
	if(Address != 0 && tmsg.Dest != Address && tmsg.Dest != 0) return false;

#if MSG_DEBUG
	// 调试版不过滤序列号为0的重复消息
	if(tmsg.Sequence != 0)
#endif
	{
		// Ack的包有可能重复，不做处理。正式响应包跟前面的Ack有相同的源地址和序列号
		if(!tmsg.Ack)
		{
			// 处理重复消息。加上来源地址，以免重复
			ushort seq = (tmsg.Src << 8) | tmsg.Sequence;
			if(_Ring.Check(seq))
			{
				// 快速响应确认消息，避免对方无休止的重发
				if(!tmsg.NoAck) AckResponse(tmsg);
#if MSG_DEBUG
				msg_printf("重复消息 Reply=%d Ack=%d Src=0x%02x Seq=%d Retry=%d\r\n", tmsg.Reply, tmsg.Ack, tmsg.Src, tmsg.Sequence, tmsg.Retry);
#else
				msg_printf("重复消息 Reply=%d Ack=%d Src=0x%02x Seq=%d\r\n", tmsg.Reply, tmsg.Ack, tmsg.Src, tmsg.Sequence);
#endif
				return false;
			}
			_Ring.Push(seq);
		}
	}

#if MSG_DEBUG
	// 尽量在Ack以后再输出日志，加快Ack处理速度
	//ShowMessage(tmsg, false, port);
#endif

	// 广播的响应消息也不要
	if(tmsg.Dest == 0 && tmsg.Reply) return false;

	// 如果是确认消息或响应消息，及时更新请求队列
	if(tmsg.Ack)
	{
		AckRequest(tmsg);
		// 如果只是确认消息，不做处理
		return false;
	}
	// 响应消息顺道帮忙消除Ack
	if(tmsg.Reply) AckRequest(tmsg);

	// 快速响应确认消息，避免对方无休止的重发
	if(!tmsg.NoAck) AckResponse(tmsg);

#if MSG_DEBUG
	// 尽量在Ack以后再输出日志，加快Ack处理速度
	ShowMessage(tmsg, false, Port);
#endif

	Total.Receive++;

	return true;
}

// 处理收到的Ack包
void TinyController::AckRequest(TinyMessage& msg)
{
	for(int i=0; i<ArrayLength(_Queue); i++)
	{
		MessageNode& node = _Queue[i];
		if(node.Using && node.Sequence == msg.Sequence)
		{
			int cost = (int)(Time.Current() - node.LastSend);
			if(cost < 0) cost = -cost;

			Total.Cost += cost;
			Total.Ack++;
			Total.Bytes += node.Length;

			// 发送开支作为新的随机延迟时间，这样子延迟重发就可以根据实际情况动态调整
			uint it = (Interval + cost) >> 1;
			// 确保小于等于超时时间的四分之一，让其有机会重发
			uint tt = Timeout >> 2;
			if(it > tt) it = tt;
			Interval = it;

			// 该传输口收到响应，从就绪队列中删除
			//_Queue.Remove(node);
			//delete node;
			node.Using = 0;

			/*if(msg.Ack)
				msg_printf("收到Ack确认包 ");
			else
				msg_printf("收到Reply确认 ");

#if MSG_DEBUG
			msg_printf("Src=%d Seq=%d Cost=%dus Retry=%d\r\n", msg.Src, msg.Sequence, cost, msg.Retry);
#else
			msg_printf("Src=%d Seq=%d Cost=%dus\r\n", msg.Src, msg.Sequence, cost);
#endif
			*/return;
		}
	}

	//if(msg.Ack) msg_printf("无效Ack确认包 Src=%d Seq=%d 可能你来迟了，消息已经从发送队列被删除\r\n", msg.Src, msg.Sequence);
}

// 向对方发出Ack包
void TinyController::AckResponse(TinyMessage& msg)
{
	TinyMessage msg2(msg);
	msg2.Src = Address;
	msg2.Dest = msg.Src;
	msg2.Reply = 1;
	msg2.Ack = 1;
	msg2.Length = 0;
#if MSG_DEBUG
	msg2.Retry = msg.Retry; // 说明这是匹配对方的哪一次重发
#endif

	//msg_printf("发送Ack确认包 Dest=0x%02x Seq=%d ", msg.Src, msg.Sequence);

	bool rs = Controller::Send(msg2);
	msg_printf("发送Ack确认包 Dest=0x%02x Seq=%d ", msg.Src, msg.Sequence);
#if MSG_DEBUG
	msg_printf("Retry=%d ", msg.Retry);
#endif
	if(rs)
		msg_printf(" 成功!\r\n");
	else
		msg_printf(" 失败!\r\n");
}

uint TinyController::Post(byte dest, byte code, byte* buf, uint len)
{
	TinyMessage msg(code);
	msg.Dest = dest;
	msg.SetData(buf, len);

	return Send(msg);
}

// 发送消息，传输口参数为空时向所有传输口发送消息
bool TinyController::Send(Message& msg)
{
	TinyMessage& tmsg = (TinyMessage&)msg;

	// 附上自己的地址
	tmsg.Src = Address;

	// 附上序列号。响应消息保持序列号不变
	if(!tmsg.Reply) tmsg.Sequence = ++_Sequence;

#if MSG_DEBUG
	// 计算校验
	msg.ComputeCrc();

	ShowMessage(tmsg, true, Port);
#endif

	//return Controller::Send(msg, port);

	return Post(tmsg, -1) ? 1 : 0;
}

void SendTask(void* param)
{
	assert_ptr(param);
	TinyController* control = (TinyController*)param;
	control->Loop();
}

void TinyController::Loop()
{
	int count;
	for(int i=0; i<ArrayLength(_Queue); i++)
	{
		MessageNode& node = _Queue[i];
		if(!node.Using) continue;

		// 检查时间。至少发送一次
		if(node.Next > 0)
		{
			// 下一次发送时间还没到，跳过
			if(node.Next > Time.Current()) continue;

			// 已过期则删除
			if(node.Expired < Time.Current())
			{
				//_Queue.Remove(node);
				//delete node;
				node.Using = 0;

				continue;
			}
		}

		count++;
		node.Times++;

#if MSG_DEBUG
		// 第6个字节表示长度
		TinyMessage* msg = (TinyMessage*)node.Data;
		// 最后一个附加字节记录第几次重发
		if(node.Length > TinyMessage::MinSize + msg->Length) node.Data[node.Length - 1] = node.Times;
#endif

		// 发送消息
		Port->Write(node.Data, node.Length);

		// 增加发送次数统计
		Total.Send++;

		// 分组统计
		if(Total.Send >= 10)
		{
			memcpy(&Last, &Total, sizeof(Total));
			memset(&Total, 0, sizeof(Total));
		}

		ulong now = Time.Current();
		node.LastSend = now;

		// 随机延迟。随机数1~5。每次延迟递增
		byte rnd = (uint)now % 3;
		node.Interval = (rnd + 1) * Interval;
		node.Next = now + node.Interval;
	}

	if(count == 0)
	{
		//debug_printf("TinyNet::Loop 消息队列为空，禁用任务\r\n");
		Sys.SetTask(_taskID, false);
	}
}

// 发送消息，usTimeout微秒超时时间内，如果对方没有响应，会重复发送，-1表示采用系统默认超时时间Timeout
bool TinyController::Post(TinyMessage& msg, int expire)
{
	// 如果没有传输口处于打开状态，则发送失败
	if(!Port->Open()) return false;

	if(expire < 0) expire = Timeout;
	// 需要响应
	if(expire == 0) msg.NoAck = true;
	// 如果确定不需要响应，则改用Post
	if(msg.NoAck || msg.Ack) return Controller::Send(msg);

	// 准备消息队列
	MessageNode* node = NULL;
	for(int i=0; i<ArrayLength(_Queue); i++)
	{
		if(!_Queue[i].Using)
		{
			node = &_Queue[i];
			node->Sequence = 0;
			node->Using = 1;
			break;
		}
	}
	// 队列已满
	if(!node) return false;

	node->SetMessage(msg);
	node->StartTime = Time.Current();
	node->Next = 0;
	node->Expired = Time.Current() + expire;

	Total.Msg++;

	// 加入等待队列
	//if(_Queue.Add(node) < 0) return false;

	Sys.SetTask(_taskID, true);

	return true;
}

// 广播消息，不等待响应和确认
bool TinyController::Broadcast(TinyMessage& msg)
{
	msg.NoAck = true;
	msg.Src = Address;
	return Post(msg, 0);
}

bool TinyController::Reply(Message& msg)
{
	TinyMessage& tmsg = (TinyMessage&)msg;

	// 回复信息，源地址变成目的地址
	tmsg.Dest = tmsg.Src;
	msg.Reply = 1;

	return Send(msg);
}

void StatTask(void* param)
{
	assert_ptr(param);
	TinyController* control = (TinyController*)param;
	control->ShowStat();
}

// 显示统计信息
void TinyController::ShowStat()
{
	static uint lastSend = 0;
	static uint lastReceive = 0;

	int tsend = Total.Send;
	if(tsend == lastSend && Total.Receive == lastReceive) return;
	lastSend = tsend;
	lastReceive = Total.Receive;

	uint rate = (Last.Ack + Total.Ack) * 100 / (Last.Send + tsend);
	uint cost = (Last.Cost + Total.Cost) / (Last.Ack + Total.Ack);
	uint speed = (Last.Bytes + Total.Bytes) * 1000000 / (Last.Cost + Total.Cost);
	uint retry = (Last.Send + tsend) * 100 / (Last.Msg + Total.Msg);
	msg_printf("TinyController::State 成功率=%d%% 平均时间=%dus 速度=%d Byte/s 平均重发=%d.%02d 收发=%d/%d \r\n", rate, cost, speed, retry/100, retry%100, Last.Receive + Total.Receive, Last.Msg + Total.Msg);
}

void MessageNode::SetMessage(TinyMessage& msg)
{
	Sequence = msg.Sequence;
	Interval = 0;
	Times = 0;
	LastSend = 0;

	// 注意，此时指针位于0，而内容长度为缓冲区长度
	Stream ms(Data, ArrayLength(Data));
	msg.Write(ms);
	Length = ms.Position();
}

RingQueue::RingQueue()
{
	Index = 0;
	ArrayZero(Arr);
	Expired = 0;
}

void RingQueue::Push(ushort item)
{
	Arr[Index++] = item;
	if(Index == ArrayLength(Arr)) Index = 0;

	// 更新过期时间，10秒
	Expired = Time.Current() + 10000000;
}

int RingQueue::Find(ushort item)
{
	for(int i=0; i < ArrayLength(Arr); i++)
	{
		if(Arr[i] == item) return i;
	}

	return -1;
}

bool RingQueue::Check(ushort item)
{
	// Expired为0说明还没有添加任何项
	if(!Expired) return false;

	// 首先检查是否过期。如果已过期，说明很长时间都没有收到消息
	if(Expired < Time.Current())
	{
		//debug_printf("环形队列过期，清空 \r\n");
		// 清空队列，重新开始
		Index = 0;
		ArrayZero(Arr);
		Expired = 0;

		return false;
	}

	// 再检查是否存在
	return Find(item) >= 0;
}
