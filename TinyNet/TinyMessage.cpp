#include "Security\Crc.h"

#include "TinyMessage.h"
#include "Security\RC4.h"

#define MSG_DEBUG DEBUG
//#define MSG_DEBUG 0
#if MSG_DEBUG
	#define msg_printf debug_printf
#else
	#define msg_printf(format, ...)
#endif

/*================================ 微网消息 ================================*/
typedef struct{
	byte Retry:4;	// 重发次数。
	//byte TTL:1;		// 路由TTL。最多3次转发
	byte NoAck:1;	// 是否不需要确认包
	byte Ack:1;		// 确认包
	byte _Error:1;	// 是否错误
	byte _Reply:1;	// 是否响应
} TFlags;

// 初始化消息，各字段为0
TinyMessage::TinyMessage(byte code) : Message(code)
{
	Data = _Data;

	// 如果地址不是8字节对齐，长整型操作会导致异常
	memset(&Dest, 0, MinSize);

	Checksum	= 0;
	Crc			= 0;
}

// 分析数据，转为消息。负载数据部分将指向数据区，外部不要提前释放内存
bool TinyMessage::Read(Stream& ms)
{
	// 消息至少4个头部字节、2字节长度和2字节校验，没有负载数据的情况下
	if(ms.Remain() < MinSize) return false;

	TS("TinyMessage::Read");

	auto p = ms.Current();
	ms.Read(&Dest, 0, HeaderSize);

	// 占位符拷贝到实际数据
	Code	= _Code;
	Length	= _Length;
	Reply	= _Reply;
	Error	= _Error;

	// 代码为0是非法的
	if(!Code)		return false;
	// 没有源地址是很不负责任的
	if(!Src)		return false;
	// 源地址和目的地址相同也是非法的
	if(Dest == Src)	return false;

	// 校验剩余长度
	ushort len	= Length;
	if(ms.Remain() < len + 2) return false;

	// 避免错误指令超长，导致溢出
	if(Data == _Data && len > ArrayLength(_Data))
	{
		debug_printf("错误指令，长度 %d 大于消息数据缓冲区长度 %d \r\n", len, ArrayLength(_Data));
		return false;
	}
	if(len > 0) ms.Read(Data, 0, len);

	// 读取真正的校验码
	Checksum = ms.ReadUInt16();

	// 计算Crc之前，需要清零TTL和Retry
	byte fs		= p[3];
	auto flag	= (TFlags*)&p[3];
	//flag->TTL	= 0;
	flag->Retry	= 0;
	// 连续的，可以直接计算Crc16
	Crc = Crc::Hash16(Array(p, HeaderSize + Length));
	// 还原数据
	p[3] = fs;

	return true;
}

void TinyMessage::Write(Stream& ms) const
{
	assert_param2(Code, "微网指令码不能为空");
	assert_param2(Src, "微网源地址不能为空");
	//assert_param2(Src != Dest, "微网目的地址不能等于源地址");
	if(Src == Dest) return;

	TS("TinyMessage::Write");

	ushort len	= Length;
	// 实际数据拷贝到占位符
	auto p = (TinyMessage*)this;
	p->_Code	= Code;
	p->_Length	= len;
	p->_Reply	= Reply;
	p->_Error	= Error;

	auto buf = ms.Current();
	// 不要写入验证码
	ms.Write(&Dest, 0, HeaderSize);
	if(len > 0) ms.Write(Data, 0, len);

	// 计算Crc之前，需要清零TTL和Retry
	byte fs		= buf[3];
	auto flag	= (TFlags*)&buf[3];
	//flag->TTL	= 0;
	flag->Retry	= 0;

	p->Checksum = p->Crc = Crc::Hash16(Array(buf, HeaderSize + len));

	// 还原数据
	buf[3] = fs;

	// 写入真正的校验码
	ms.Write(Checksum);
}

// 验证消息校验码是否有效
bool TinyMessage::Valid() const
{
	if(Checksum == Crc) return true;

	msg_printf("Message::Valid Crc Error %04X != Checksum: %04X \r\n", Crc, Checksum);
#if MSG_DEBUG
	msg_printf("指令校验错误 ");
	Show();
#endif

	return false;
}

// 消息所占据的指令数据大小。包括头部、负载数据和附加数据
uint TinyMessage::Size() const
{
	return MinSize + Length;
}

uint TinyMessage::MaxDataSize() const
{
	return Data == _Data ? ArrayLength(_Data) : Length;
}

void TinyMessage::Show() const
{
#if MSG_DEBUG
	assert_ptr(this);

	byte flag	= *((byte*)&_Code + 1);
	msg_printf("0x%02X => 0x%02X Code=0x%02X Flag=0x%02X Seq=0x%02X Retry=%d", Src, Dest, Code, flag, Seq, Retry);

	if(flag)
	{
		if(Reply)	msg_printf(" Reply");
		if(Error)	msg_printf(" Error");
		if(Ack)		msg_printf(" Ack");
		if(NoAck)	msg_printf(" NoAck");
	}

	ushort len	= Length;
	if(len > 0)
	{
		assert_ptr(Data);
		msg_printf(" Data[%02d]=", len);
		ByteArray(Data, len).Show();
	}
	if(Checksum != Crc) msg_printf(" Crc Error 0x%04x [%04X]", Crc, __REV16(Crc));

	msg_printf("\r\n");
#endif
}

TinyMessage TinyMessage::CreateReply() const
{
	TinyMessage msg;
	msg.Dest	= Src;
	msg.Src		= Dest;
	msg.Code	= Code;
	msg.Reply	= true;
	msg.Seq		= Seq;
	msg.State	= State;
#if MSG_DEBUG
	msg.Retry	= Retry;
#endif

	return msg;
}

/*================================ 微网控制器 ================================*/
void SendTask(void* param);
void StatTask(void* param);

// 构造控制器
TinyController::TinyController() : Controller()
{
	MinSize	= TinyMessage::MinSize;

	// 初始化一个随机地址
	Address = Sys.ID[0];
	// 如果地址为0，则使用时间来随机一个
	// 节点地址范围2~254，网关专用0x01，节点让步
	while(Address < 2 || Address > 254)
	{
		Sys.Delay(30);
		Address = Sys.Ms();
	}

	// 接收模式。0只收自己，1接收自己和广播，2接收所有。默认0
	Mode		= 0;
	Interval	= 20;
	Timeout		= 200;
	auto cfg	= TinyConfig::Current;
	if(cfg)
	{
		// 调整重发参数
		if(cfg->New && cfg->Interval == 0)
		{
			cfg->Interval	= Interval;
			cfg->Timeout	= Timeout;
		}
		Interval	= cfg->Interval;
		Timeout		= cfg->Timeout;
		cfg->Address= Address;
	}

	_taskID		= 0;
	_Queue		= NULL;
	QueueLength	= 8;
}

TinyController::~TinyController()
{
	Sys.RemoveTask(_taskID);

	delete[] _Queue;
	_Queue	= NULL;
}

void TinyController::Open()
{
	if(Opened) return;

	assert_param2(Port, "还没有传输口呢");

	debug_printf("TinyNet::Inited Address=%d (0x%02X) 使用传输接口 %s\r\n", Address, Address, Port->ToString());
	debug_printf("\t间隔: %dms\r\n", Interval);
	debug_printf("\t超时: %dms\r\n", Timeout);
	debug_printf("\t模式: %d ", Mode);
	// 接收模式。0只收自己，1接收自己和广播，2接收所有。默认0
	switch(Mode)
	{
		case 0:
			debug_printf("仅本地址");
			break;
		case 1:
			debug_printf("本地址和广播");
			break;
		case 2:
			debug_printf("接收所有");
			break;
	}
	debug_printf("\r\n");

	Controller::Open();

	// 初始化发送队列
	_Queue	= new MessageNode[QueueLength];
	memset(_Queue, 0, QueueLength);

	if(!_taskID)
	{
		_taskID = Sys.AddTask(SendTask, this, 0, 1, "微网队列");
		// 默认禁用，有数据要发送才开启
		Sys.SetTask(_taskID, false);
	}

	//memset(&Total, 0, sizeof(TinyStat));
	//memset(&Last, 0, sizeof(TinyStat));

#if MSG_DEBUG
	Sys.AddTask(StatTask, this, 1000, 15000, "微网统计");
#endif
}

void ShowMessage(const TinyMessage& msg, bool send, ITransport* port)
{
	if(msg.Ack) return;

	int blank = 6;
	msg_printf("%s", port->ToString());
	if(send && !msg.Reply)
	{
		msg_printf("::Send ");
		blank -= 5;
	}
	else
		msg_printf("::");

	if(msg.Error)
	{
		msg_printf("Error ");
		blank -= 6;
	}
	else if(msg.Ack)
	{
		msg_printf("Ack ");
		blank -= 4;
	}
	else if(msg.Reply)
	{
		msg_printf("Reply ");
		blank -= 6;
	}
	else if(!send)
	{
		msg_printf("Recv ");
		blank -= 5;
	}
	if(blank > 0)
	{
		String str(' ', blank);
		str.Show();
	}

	// 显示目标地址
	auto st	= msg.State;
	if(st)
	{
		msg_printf("Mac=");
		if(strcmp(port->ToString(), "R24") == 0)
			ByteArray(st, 5).Show();
		else if(strcmp(port->ToString(), "ShunCom") == 0)
			ByteArray(st, 2).Show();
		else
			ByteArray(st, 6).Show();
		msg_printf(" ");
	}

	msg.Show();
}

//加密。组网不加密，退网不加密
static bool Encrypt(Message& msg,  Array& pass)
{
	// 加解密。组网不加密，退网不加密
	if(msg.Length > 0 && pass.Length() > 0 && !(msg.Code == 0x01 || msg.Code == 0x02))
	{
		Array bs(msg.Data, msg.Length);
		RC4::Encrypt(bs, pass);
		return true;
	}
	return false;
}

bool TinyController::Dispatch(Stream& ms, Message* pmsg, void* param)
{
	/*byte* buf	= ms.Current();
	// 前移一个字节，确保不是自己的消息时，数据流能够移动
	ms.Seek(1);

	// 代码为0是非法的
	if(!buf[2]) return true;
	// 没有源地址是很不负责任的
	if(!buf[1]) return true;
	// 非广播包时，源地址和目的地址相同也是非法的
	if(buf[0] == buf[1]) return false;
	// 源地址是自己不要接收
	if(buf[1] == Address) return true;
	// 只处理本机消息或广播消息
	//if(tmsg.Dest != Address && tmsg.Dest != 0) return false;
	// 不是自己的消息，并且没有设置接收全部
	if(Mode < 2 && buf[0] != Address)
	{
		// 如果不是广播或者不允许广播
		if(Mode != 1 || buf[0] != 0) return true;
	}

	// 后移一个字节来弥补
	ms.Seek(-1);*/
	TinyMessage msg;
	return Controller::Dispatch(ms, &msg, param);
}

// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
bool TinyController::Valid(const Message& _msg)
{
	auto& msg = (TinyMessage&)_msg;

	// 代码为0是非法的
	if(!msg.Code) return false;
	// 没有源地址是很不负责任的
	if(!msg.Src) return false;
	//if(msg!=Address) return false;
	// 非广播包时，源地址和目的地址相同也是非法的
	if(msg.Dest == msg.Src) return false;
	// 源地址是自己不要接收
	if(msg.Src == Address) return false;
	// 只处理本机消息或广播消息
	//if(msg.Dest != Address && msg.Dest != 0) return false;
	// 不是自己的消息，并且没有设置接收全部
	if(Mode < 2 && msg.Dest != Address)
	{
		// 如果不是广播或者不允许广播
		if(Mode != 1 || msg.Dest != 0) return false;
	}

	TS("TinyController::Valid");

#if MSG_DEBUG
	// 调试版不过滤序列号为0的重复消息
	if(msg.Seq != 0)
#endif
	{
		// Ack的包有可能重复，不做处理。正式响应包跟前面的Ack有相同的源地址和序列号
		if(!msg.Ack)
		{
			// 处理重复消息。加上来源地址，以免重复
			ushort seq = (msg.Src << 8) | msg.Seq;
			if(_Ring.Check(seq))
			{
				// 对方可能多次发同一个请求过来，都要做响应
				if(!msg.Reply && AckResponse(msg)) return false;

				msg_printf("重复消息 Src=0x%02x Code=0x%02X Seq=0x%02X Retry=%d Reply=%d Ack=%d\r\n", msg.Src, msg.Code, msg.Seq, msg.Retry, msg.Reply, msg.Ack);
				return false;
			}
			_Ring.Push(seq);
		}
	}

	// 广播的响应消息也不要
	if(msg.Dest == 0 && msg.Reply) return false;

	// 仅处理本节点的确认消息
	if(msg.Dest == Address)
	{
		// 如果是确认消息或响应消息，及时更新请求队列
		if(msg.Ack)
		{
			AckRequest(msg);
			// 如果只是确认消息，不做处理
			return false;
		}
		// 响应消息消除请求，请求消息要求重发响应
		if(msg.Reply)
			AckRequest(msg);
		else
		{
			if(AckResponse(msg))
			{
				debug_printf("匹配请求 ");
				return false;
			}
		}
	}

	if(msg.Dest == Address)
	{
		ByteArray  key(0);
		GetKey(msg.Src, key, Param);
		if(key.Length() > 0) Encrypt(msg, key);
	}

#if MSG_DEBUG
	// 尽量在Ack以后再输出日志，加快Ack处理速度
	ShowMessage(msg, false, Port);
#endif

	Total.Receive++;

	return true;
}

// 处理收到的响应包或Ack包，消除请求
void TinyController::AckRequest(const TinyMessage& msg)
{
	if(!msg.Ack && !msg.Reply) return;

	for(int i=0; i<QueueLength; i++)
	{
		auto& node = _Queue[i];
		if(node.Using && node.Seq == msg.Seq)
		{
			int cost = Sys.Ms() - node.StartTime;
			if(cost < 0) cost = -cost;

			Total.Cost	+= cost;
			Total.Success++;
			Total.Bytes	+= node.Length;

			// 该传输口收到响应，从就绪队列中删除
			node.Using = 0;

			if(msg.Ack)
				msg_printf("收到确认 ");
			else
				msg_printf("响应确认 ");

			msg_printf("Src=0x%02x Code=0x%02X Seq=0x%02X Retry=%d Cost=%dms \r\n", msg.Src, msg.Code, msg.Seq, msg.Retry, cost);
			return;
		}
	}

#if MSG_DEBUG
	if(msg.Ack)
	{
		msg_printf("无效确认 ");
		ShowMessage(msg, false, Port);
	}
#endif
}

// 处理对方发出的请求，如果已响应则重发响应
bool TinyController::AckResponse(const TinyMessage& msg)
{
	if(msg.Reply) return false;
	// 广播消息不要给确认
	if(msg.Dest == 0) return false;

	TS("TinyController::AckResponse");

	for(int i=0; i<QueueLength; i++)
	{
		auto& node = _Queue[i];
		if(node.Using && node.Seq == msg.Seq)
		{
			// 马上重发这条响应
			node.Next	= 0;

			Sys.SetTask(_taskID, true, 0);

			msg_printf("重发响应 ");
			msg_printf("Src=0x%02x Code=0x%02X Seq=0x%02X Retry=%d \r\n", msg.Src, msg.Code, msg.Seq, msg.Retry);
			return true;
		}
	}

	return false;
}

static byte _Sequence	= 0;
// 发送消息，传输口参数为空时向所有传输口发送消息
bool TinyController::Send(Message& _msg)
{
	auto& msg = (TinyMessage&)_msg;

	// 附上自己的地址
	msg.Src = Address;

	// 附上序列号。响应消息保持序列号不变
	if(!msg.Reply) msg.Seq = ++_Sequence;

	ByteArray  key;
	GetKey(msg.Dest, key, Param);
	if(key.Length() > 0) Encrypt(msg, key);

#if MSG_DEBUG
	ShowMessage(msg, true, Port);
#endif

	return Post(msg, -1);
}

bool TinyController::Reply(Message& _msg)
{
	auto& msg = (TinyMessage&)_msg;

	// 回复信息，源地址变成目的地址
	if(msg.Dest == Address && msg.Src != Address) msg.Dest = msg.Src;
	msg.Reply = 1;

	return Send(msg);
}

// 广播消息，不等待响应和确认
bool TinyController::Broadcast(TinyMessage& msg)
{
	TS("TinyController::Broadcast");

	msg.NoAck	= true;
	msg.Src		= Address;
	msg.Dest	= 0;

	// 附上序列号。响应消息保持序列号不变
	if(!msg.Reply) msg.Seq = ++_Sequence;

#if MSG_DEBUG
	ShowMessage(msg, true, Port);
#endif

	if(!Port->Open()) return false;

	Total.Broadcast++;

	return Controller::SendInternal(msg);
}

// 放入发送队列，超时之前，如果对方没有响应，会重复发送，-1表示采用系统默认超时时间Timeout
bool TinyController::Post(const TinyMessage& msg, int msTimeout)
{
	TS("TinyController::Post");

	// 如果没有传输口处于打开状态，则发送失败
	if(!Port->Open()) return false;

	if(msTimeout < 0) msTimeout = Timeout;
	// 如果确定不需要响应，则改用Post
	if(msTimeout <= 0 || msg.NoAck || msg.Ack)
	{
		Total.Broadcast++;
		return Controller::SendInternal(msg);
	}

	// 针对Zigbee等不需要Ack确认的通道
	if(Timeout < 0)
	{
		Total.Broadcast++;
		return Controller::SendInternal(msg);
	}

	// 准备消息队列
	auto now	= Sys.Ms();
	int idx		= -1;
	for(int i=0; i<QueueLength; i++)
	{
		auto& node = _Queue[i];
		// 未使用，或者即使使用也要抢走已过期的节点
		if(!node.Using || node.EndTime < now)
		{
			node.Seq	= 0;
			node.Using	= 1;
			idx	= i;
			break;
		}
	}
	// 队列已满
	if(idx < 0)
	{
		debug_printf("TinyController::Post 发送队列已满！ \r\n");
		return false;
	}

	_Queue[idx].Set(msg, msTimeout);

	if(msg.Reply)
		Total.Reply++;
	else
		Total.Msg++;

	Sys.SetTask(_taskID, true, 0);

	return true;
}

void SendTask(void* param)
{
	assert_ptr(param);

	auto control = (TinyController*)param;
	control->Loop();
}

void TinyController::Loop()
{
	TS("TinyController::Loop");

	/*
	队列机制：
	1，定时发送请求消息，直到被响应消去或者超时
	2，发送一次响应消息，直到超时，中途有该序列的请求则直接重发响应
	3，不管请求还是响应，同样方式处理超时，响应的下一次时间特别长
	4，如果某个请求序列对应的响应存在，则直接将其下一次时间置为0，让其马上重发
	5，发送响应消息，不好统计速度和成功率
	6，发送次数不允许超过5次
	*/

	int count = 0;
	for(int i=0; i<QueueLength; i++)
	{
		auto& node = _Queue[i];
		if(!node.Using || node.Seq == 0) continue;

		auto flag = (TFlags*)&node.Data[3];
		bool reply	= flag->_Reply;
		// 可用请求消息数，需要继续轮询
		if(!reply) count++;

		// 检查时间。至少发送一次
		if(node.Times > 0)
		{
			ulong now = Sys.Ms();

			// 已过期则删除
			if(node.EndTime < now || node.Times > 50)
			{
				if(!reply) msg_printf("消息过期 Dest=0x%02X Seq=0x%02X Times=%d\r\n", node.Data[0], node.Seq, node.Times);
				node.Using	= 0;
				node.Seq	= 0;

				continue;
			}

			// 下一次发送时间还没到，跳过
			if(node.Next > now) continue;

			//msg_printf("重发消息 Dest=0x%02X Seq=0x%02X Times=%d\r\n", node.Data[0], node.Seq, node.Times + 1);
		}

		node.Times++;

		// 发送消息
		Array bs(node.Data, node.Length);
		if(node.Length > 32)
		{
			debug_printf("node=0x%08x Length=%d Seq=0x%02X Times=%d Next=%d EndTime=%d\r\n", &node, node.Length, node.Seq, node.Times, (uint)node.Next, (uint)node.EndTime);
		}
		if(node.Mac[0])
			Port->Write(bs, &node.Mac[1]);
		else
			Port->Write(bs);

		// 递增重试次数
		flag->Retry++;

		// 请求消息列入统计
		if(!reply)
		{
			// 增加发送次数统计
			Total.Send++;

			// 分组统计
			if(Total.Send >= 1000)
			{
				//memcpy(&Last, &Total, sizeof(TinyStat));
				//memset(&Total, 0, sizeof(TinyStat));
				Last	= Total;
				Total.Clear();
			}
		}

		// 计算下一次重发时间
		{
			ulong now	= Sys.Ms();
			//node.LastSend	= now;

			// 随机延迟。随机数1~5。每次延迟递增
			//byte rnd	= (uint)now % 3;
			//node.Next	= now + (rnd + 1) * Interval;
			if(!reply)
				node.Next	= now + Interval;
			else
				node.Next	= now + 60000;
		}
	}

	// 没有可用请求时，停止轮询
	if(count == 0) Sys.SetTask(_taskID, false);
}

void StatTask(void* param)
{
	assert_ptr(param);

	auto control = (TinyController*)param;
	control->ShowStat();
}

// 显示统计信息
void TinyController::ShowStat() const
{
#if MSG_DEBUG
	static uint lastSend = 0;
	static uint lastReceive = 0;

	int tsend	= Total.Send;
	if(tsend == lastSend && Total.Receive == lastReceive) return;
	lastSend	= tsend;
	lastReceive	= Total.Receive;

	uint rate	= 100;
	uint tack	= Last.Success + Total.Success;
	uint tmsg	= Last.Msg + Total.Msg;
	tsend		+= Last.Send;
	if(tsend > 0)
		rate	= tack * 100 / tmsg;
	uint cost	= 0;
	if(tack > 0)
		cost	= (Last.Cost + Total.Cost) / tack;
	uint speed	= 0;
	if(Last.Cost + Total.Cost > 0)
		speed	= (Last.Bytes + Total.Bytes) * 1000 / (Last.Cost + Total.Cost);
	uint retry	= 0;
	if(Last.Msg + Total.Msg > 0)
		retry	= tsend * 100 / tmsg;
	msg_printf("Tiny::State 成功=%d%% %d/%d 平均=%dms 速度=%d Byte/s 次数=%d.%02d 接收=%d 响应=%d 广播=%d \r\n", rate, tack, tmsg, cost, speed, retry/100, retry%100, Last.Receive + Total.Receive, Last.Reply + Total.Reply, Last.Broadcast + Total.Broadcast);
#endif
}

/*================================ 信息节点 ================================*/
void MessageNode::Set(const TinyMessage& msg, int msTimeout)
{
	Times		= 0;
	//LastSend	= 0;

	ulong now	= Sys.Ms();
	StartTime	= now;
	EndTime		= now + msTimeout;

	Next		= 0;
	// 响应消息只发一次，然后长时间等待
	if(msg.Reply) Next	= now + 60000;

	// 注意，此时指针位于0，而内容长度为缓冲区长度
	Stream ms(Data, ArrayLength(Data));
	msg.Write(ms);
	Length		= ms.Position();
	if(Length > 32) debug_printf("Length=%d \r\n", Length);

	Mac[0]		= 0;
	if(msg.State)
	{
		Mac[0]	= 5;
		memcpy(&Mac[1], msg.State, 5);
	}
	Seq			= msg.Seq;

	//debug_printf("Set 0x%08x \r\n", this);
}

/*================================ 环形队列 ================================*/
RingQueue::RingQueue()
{
	Index	= 0;
	ArrayZero(Arr);
	Expired	= 0;
}

void RingQueue::Push(ushort item)
{
	Arr[Index++] = item;
	if(Index == ArrayLength(Arr)) Index = 0;

	// 更新过期时间，10秒
	Expired = Sys.Ms() + 10000;
}

int RingQueue::Find(ushort item) const
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
	if(Expired < Sys.Ms())
	{
		//msg_printf("环形队列过期，清空 \r\n");
		// 清空队列，重新开始
		Index	= 0;
		ArrayZero(Arr);
		Expired	= 0;

		return false;
	}

	// 再检查是否存在
	return Find(item) >= 0;
}

TinyStat::TinyStat()
{
	Clear();
}

TinyStat& TinyStat::operator=(const TinyStat& ts)
{
	memcpy(this, &ts, sizeof(this[0]));

	return *this;
}

void TinyStat::Clear()
{
	memset(this, 0, sizeof(this[0]));
}
