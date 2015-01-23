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
#if DEBUG
	Retry = 1;
#endif
}

TinyMessage::TinyMessage(TinyMessage& msg) : Message(msg)
{
	Data = _Data;

	memcpy(&Dest, &msg.Dest, MinSize);

	Crc = msg.Crc;
	//ArrayCopy(Data, msg.Data);
	if(Length) memcpy(Data, msg.Data, Length);
	TTL = msg.TTL;
#if DEBUG
	Retry = msg.Retry;
#endif
}

// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
bool TinyMessage::Read(MemoryStream& ms)
{
	// 消息至少4个头部字节、2字节长度和2字节校验，没有负载数据的情况下
	if(ms.Remain() < MinSize) return false;

	ms.Read(&Dest, 0, HeaderSize);
	// 占位符拷贝到实际数据
	Code = _Code;
	Length = _Length;

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
#if DEBUG
	// 调试诊断模式下该字段表示第几次重发
	if(ms.Remain() > 0)
		Retry = ms.Read<byte>();
	else
		Retry = 0;
#endif

	return true;
}

void TinyMessage::ComputeCrc()
{
	MemoryStream ms(Size());

	Write(ms);

	// 扣除不计算校验码的部分
	Checksum = Crc = Sys.Crc16(ms.GetBuffer(), HeaderSize + Length);
}

// 验证消息校验码是否有效
bool TinyMessage::Valid() const
{
	if(Checksum == Crc) return true;

	debug_printf("Message::Valid Crc Error %04X != Checksum: %04X \r\n", Crc, Checksum);
	return false;
}

// 消息所占据的指令数据大小。包括头部、负载数据和附加数据
uint TinyMessage::Size() const
{
	uint len = MinSize + Length;
	if(UseTTL) len++;
#if DEBUG
	len++;
#endif
	return len;
}

void TinyMessage::Write(MemoryStream& ms)
{
	// 实际数据拷贝到占位符
	_Code = Code;
	_Length = Length;

	byte* buf = ms.Current();
	// 不要写入验证码
	ms.Write((byte*)&Dest, 0, HeaderSize);
	if(Length > 0) ms.Write(Data, 0, Length);

	Checksum = Crc = Sys.Crc16(buf, HeaderSize + Length);
	// 读取真正的校验码
	ms.Write(Checksum);

	// 后面可能有TTL
	if(UseTTL) ms.Write(TTL);
#if DEBUG
	ms.Write(Retry);
#endif
}

// 构造控制器
TinyController::TinyController(ITransport* port) : Controller(port)
{
	Init();
}

TinyController::TinyController(ITransport* ports[], int count) : Controller(ports, count)
{
	Init();
}

void TinyController::Init()
{
	_Sequence	= 0;
	_taskID		= 0;
	Interval	= 8000;
	Timeout		= 200;

	MinSize = TinyMessage::MinSize;
	
	// 初始化一个随机地址
	Address = Sys.ID[0];
	// 如果地址为0，则使用时间来随机一个
	//while(!Address) Address = Time.Current();
	// 节点地址范围2~254，网关专用0x01，节点让步
	while(Address < 2 || Address > 254)
	{
		Time.Sleep(10);
		Address = Time.Current();
	}

	//debug_printf("TinyNet::Inited Address=%d (0x%02x) 使用[%d]个传输接口\r\n", Address, Address, _ports.Count());

	if(!_taskID)
	{
		debug_printf("TinyNet::微网控制器消息发送队列 ");
		_taskID = Sys.AddTask(SendTask, this, 0, 1000);
	}

	TotalSend = TotalAck = TotalBytes = TotalCost = TotalRetry = TotalMsg = 0;
	LastSend = LastAck = LastBytes = LastCost = LastRetry = LastMsg = 0;

	// 因为统计不准确，暂时不显示状态统计
	//Sys.AddTask(StatTask, this, 1000000, 5000000);
}

TinyController::~TinyController()
{
	if(_taskID) Sys.RemoveTask(_taskID);
}

// 创建消息
Message* TinyController::Create() const
{
	return new TinyMessage();
}

void ShowMessage(TinyMessage& msg, bool send, ITransport* port = NULL)
{
	if(msg.Ack) return;

	//msg_printf("%d ", (uint)Time.Current());
	if(send)
		msg_printf("TinyMessage::Send ");
	else
	{
		msg_printf("%s ", port->ToString());
		msg_printf("TinyMessage::");
	}
	if(msg.Error)
		msg_printf("Error");
	else if(msg.Ack)
		msg_printf("Ack");
	else if(msg.Reply)
		msg_printf("Reply");
	else if(!send)
		msg_printf("Request");

	msg_printf(" 0x%02x => 0x%02x Code=0x%02x Flag=%02x Sequence=%d Length=%d Checksum=0x%04x Retry=%d ", msg.Src, msg.Dest, msg.Code, *((byte*)&(msg.Code)+1), msg.Sequence, msg.Length, msg.Checksum, msg.Retry);
	if(msg.Length > 0)
	{
		msg_printf(" 数据：[%d] ", msg.Length);
		Sys.ShowString(msg.Data, msg.Length, false);
	}
	if(!msg.Valid()) msg_printf(" Crc Error 0x%04x [%04X]", msg.Crc, __REV16(msg.Crc));
	msg_printf("\r\n");

	/*Sys.ShowHex(buf, MESSAGE_SIZE);
	if(msg.Length > 0)
		Sys.ShowHex(msg.Data, msg.Length);
	msg_printf("\r\n");*/
}

// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
bool TinyController::OnReceive(Message& msg, ITransport* port)
{
	TinyMessage& tmsg = (TinyMessage&)msg;

	// 代码为0是非法的
	if(!msg.Code) return false;
	// 没有源地址是很不负责任的
	if(!tmsg.Src) return false;
	// 非广播包时，源地址和目的地址相同也是非法的
	if(tmsg.Dest == tmsg.Src) return false;
	// 只处理本机消息或广播消息。快速处理，高效。
	if(tmsg.Dest != Address && tmsg.Dest != 0) return false;

#if DEBUG
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
				if(!tmsg.NoAck) AckResponse(tmsg, port);
#if DEBUG
				msg_printf("重复消息 Reply=%d Ack=%d Src=%d Seq=%d Retry=%d\r\n", tmsg.Reply, tmsg.Ack, tmsg.Src, tmsg.Sequence, tmsg.Retry);
#else
				msg_printf("重复消息 Reply=%d Ack=%d Src=%d Seq=%d\r\n", tmsg.Reply, tmsg.Ack, tmsg.Src, tmsg.Sequence);
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
	if(tmsg.Dest == 0 && tmsg.Reply) return true;

	// 如果是确认消息或响应消息，及时更新请求队列
	if(tmsg.Ack)
	{
		AckRequest(tmsg, port);
		// 如果只是确认消息，不做处理
		return true;
	}
	// 响应消息顺道帮忙消除Ack
	if(tmsg.Reply) AckRequest(tmsg, port);

	// 快速响应确认消息，避免对方无休止的重发
	if(!tmsg.NoAck) AckResponse(tmsg, port);

#if MSG_DEBUG
	// 尽量在Ack以后再输出日志，加快Ack处理速度
	ShowMessage(tmsg, false, port);
#endif

	return true;
}

void TinyController::AckRequest(TinyMessage& msg, ITransport* port)
{
	int i = -1;
	while(_Queue.MoveNext(i))
	{
		MessageNode* node = _Queue[i];
		if(node->Sequence == msg.Sequence)
		{
			uint cost = (uint)(Time.Current() - node->LastSend);
			if(node->Ports.Count() > 0)
			{
				TotalCost += cost;
				TotalAck++;
				TotalBytes += node->Length;
			}

			// 发送开支作为新的随机延迟时间，这样子延迟重发就可以根据实际情况动态调整
			Interval = (Interval + cost) >> 1;
			// 确保小于等于超时时间的四分之一，让其有机会重发
			uint tt = (Timeout * 1000) >> 2;
			if(Interval > tt) Interval = tt;

			// 该传输口收到响应，从就绪队列中删除
			node->Ports.Remove(port);

			/*if(msg.Ack)
				msg_printf("收到Ack确认包 ");
			else
				msg_printf("收到Reply确认 ");

#if DEBUG
			msg_printf("Src=%d Seq=%d Cost=%dus Retry=%d\r\n", msg.Src, msg.Sequence, cost, msg.Retry);
#else
			msg_printf("Src=%d Seq=%d Cost=%dus\r\n", msg.Src, msg.Sequence, cost);
#endif
			*/return;
		}
	}

	//if(msg.Ack) msg_printf("无效Ack确认包 Src=%d Seq=%d 可能你来迟了，消息已经从发送队列被删除\r\n", msg.Src, msg.Sequence);
}

void TinyController::AckResponse(TinyMessage& msg, ITransport* port)
{
	TinyMessage msg2(msg);
	msg2.Dest = msg.Src;
	msg2.Reply = 1;
	msg2.Ack = 1;
	msg2.Length = 0;
#if DEBUG
	msg2.Retry = msg.Retry; // 说明这是匹配对方的哪一次重发
#endif

	msg_printf("发送Ack确认包 Dest=%d Seq=%d ", msg.Src, msg.Sequence);

	int count = Send(msg2, port);
	if(count > 0)
		msg_printf(" 成功!\r\n");
	else
		msg_printf(" 失败!\r\n");
}

uint TinyController::Send(byte dest, byte code, byte* buf, uint len, ITransport* port)
{
	TinyMessage msg(code);
	msg.Dest = dest;
	msg.SetData(buf, len);

	return Send(msg, -1, port);
}

// 发送消息，传输口参数为空时向所有传输口发送消息
int TinyController::Send(Message& msg, ITransport* port)
{
	TinyMessage& tmsg = (TinyMessage&)msg;

	// 附上自己的地址
	tmsg.Src = Address;

	// 附上序列号。响应消息保持序列号不变
	if(!tmsg.Reply) tmsg.Sequence = ++_Sequence;

#if MSG_DEBUG
	// 计算校验
	msg.ComputeCrc();

	ShowMessage(tmsg, true);
#endif

	return Controller::Send(msg, port);
}

void SendTask(void* param)
{
	TinyController* control = (TinyController*)param;
	control->Loop();
}

void TinyController::Loop()
{
	int i = -1;
	while(_Queue.MoveNext(i))
	{
		MessageNode* node = _Queue[i];

		// 检查时间。至少发送一次
		if(node->Next > 0)
		{
			if(node->Next > Time.Current()) continue;

			// 检查是否传输口已完成，是否已过期
			int count = node->Ports.Count();
			if(count == 0 || node->Expired < Time.Current())
			{
				//if(count > 0)
				/*{
					msg_printf("删除消息 Seq=%d 共发送[%d]次 ", node->Sequence, node->Times);
					if(count == 0)
						msg_printf("已完成！\r\n");
					else
						msg_printf("超时！Interval=%dus\r\n", Interval);
				}*/
				TotalRetry = node->Times;

				_Queue.Remove(node);
				delete node;

				continue;
			}
		}

		//if(node->Times > 0) msg_printf("Seq=%d 延迟 %dus\r\n", node->Sequence, node->Interval);
		node->Times++;

#if DEBUG
		// 最后一个附加字节记录第几次重发
		node->Data[node->Length - 1] = node->Times;
#endif

		// 发送消息
		int k = -1;
		while(node->Ports.MoveNext(k))
		{
			ITransport* port = node->Ports[k];
			if(port) port->Write(node->Data, node->Length);

			// 增加发送次数统计
			TotalSend++;
			//TotalBytes += node->Length;
		}

		// 分组统计
		if(TotalSend >= 10)
		{
			LastSend	= TotalSend;
			LastAck		= TotalAck;
			LastBytes	= TotalBytes;
			LastCost	= TotalCost;
			LastRetry	= TotalRetry;
			LastMsg		= TotalMsg;

			TotalSend	= 0;
			TotalAck	= 0;
			TotalBytes	= 0;
			TotalCost	= 0;
			TotalRetry	= 0;
			TotalMsg	= 0;
		}

		node->LastSend = Time.Current();

		{
			// 随机延迟。随机数1~5。每次延迟递增
			byte rnd = (uint)Time.Current() % 3;
			node->Interval = (rnd + 1) * Interval;
			node->Next = node->LastSend + node->Interval;
		}
	}
}

// 发送消息，timerout毫秒超时时间内，如果对方没有响应，会重复发送，-1表示采用系统默认超时时间Timeout
bool TinyController::Send(TinyMessage& msg, int expire, ITransport* port)
{
	// 如果没有传输口处于打开状态，则发送失败
	if(port && !port->Open()) return false;

	if(expire < 0) expire = Timeout;
	// 需要响应
	if(expire == 0) msg.NoAck = true;
	// 如果确定不需要响应，则改用Post
	if(msg.NoAck) return Send(msg, port);

	// 准备消息队列
	MessageNode* node = new MessageNode();
	node->SetMessage(msg);
	if(!port)
		node->Ports = _ports;
	else
		node->Ports.Add(port);

	node->StartTime = Time.Current();
	node->Next = 0;
	node->Expired = Time.Current() + expire * 1000;

	TotalMsg++;

	// 加入等待队列
	if(_Queue.Add(node) < 0) return false;

	return true;
}

bool TinyController::Reply(TinyMessage& msg, ITransport* port)
{
	// 回复信息，源地址变成目的地址
	msg.Dest = msg.Src;
	msg.Reply = 1;

	return Send(msg, -1, port);
}

bool TinyController::Error(TinyMessage& msg, ITransport* port)
{
	msg.Error = 1;

	return Send(msg, 0, port);
}

void StatTask(void* param)
{
	TinyController* control = (TinyController*)param;
	control->ShowStat();
}

// 显示统计信息
void TinyController::ShowStat()
{
	static uint last = 0;
	if(TotalSend == last) return;
	last = TotalSend;

	uint rate = (LastAck + TotalAck) * 100 / (LastSend + TotalSend);
	uint cost = (LastCost + TotalCost) / (LastAck + TotalAck);
	uint speed = (LastBytes + TotalBytes) * 1000000 / (LastCost + TotalCost);
	uint retry = (LastSend + TotalSend) * 100 / (LastMsg + TotalMsg);
	msg_printf("TinyController::State 成功率=%d%% 平均时间=%dus 速度=%d Byte/s 平均重发=%d.%02d \r\n", rate, cost, speed, retry/100, retry%100);
}

void MessageNode::SetMessage(TinyMessage& msg)
{
	Sequence = msg.Sequence;
	//NoAck = msg.NoAck;
	Interval = 0;
	Times = 0;
	LastSend = 0;

	// 这种方式不支持后续的TTL等，所以不再使用
	/*byte* buf = (byte*)&msg.Dest;
	if(!msg.Length)
	{
		Length = MESSAGE_SIZE;
		memcpy(Data, buf, MESSAGE_SIZE);
	}
	else*/
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
		debug_printf("环形队列过期，清空 \r\n");
		// 清空队列，重新开始
		Index = 0;
		ArrayZero(Arr);
		Expired = 0;

		return false;
	}

	// 再检查是否存在
	return Find(item) >= 0;
}
