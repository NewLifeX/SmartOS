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
	byte Retry:2;	// 重发次数。
	byte TTL:2;		// 路由TTL。最多3次转发
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
	byte fs = p[3];
	auto f = (TFlags*)&p[3];
	f->TTL		= 0;
	f->Retry	= 0;
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
	byte fs 	= buf[3];
	auto f = (TFlags*)&buf[3];
	f->TTL		= 0;
	f->Retry	= 0;

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
	Interval	= 10;
	Timeout		= 50;

	_taskID		= 0;
	ArrayZero(_Queue);
}

TinyController::~TinyController()
{
	Sys.RemoveTask(_taskID);
}

void TinyController::Open()
{
	if(Opened) return;

	assert_param2(Port, "还没有传输口呢");

	debug_printf("TinyNet::Inited Address=%d (0x%02X) 使用传输接口 %s\r\n", Address, Address, Port->ToString());

	Controller::Open();

	if(!_taskID)
	{
		_taskID = Sys.AddTask(SendTask, this, 0, 1, "微网队列");
		// 默认禁用，有数据要发送才开启
		Sys.SetTask(_taskID, false);
	}

	memset(&Total, 0, sizeof(Total));
	memset(&Last, 0, sizeof(Last));

#if MSG_DEBUG
	Sys.AddTask(StatTask, this, 1000, 15000, "微网统计");
#endif
}

void ShowMessage(TinyMessage& msg, bool send, ITransport* port)
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

	msg.Show();
}

//加密。组网不加密，退网不加密
static bool Encrypt(Message& msg,  Array& pass)
{
	// 加解密。组网不加密，退网不加密
	if(msg.Length > 0 && pass.Length() > 0 && !(msg.Code == 0x01 || msg.Code == 0x02||msg.Code==0x03))
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
bool TinyController::Valid(const Message& msg)
{
	auto& tmsg = (TinyMessage&)msg;

	// 代码为0是非法的
	if(!msg.Code) return false;
	// 没有源地址是很不负责任的
	if(!tmsg.Src) return false;
	//if(tmsg!=Address) return false;
	// 非广播包时，源地址和目的地址相同也是非法的
	if(tmsg.Dest == tmsg.Src) return false;
	// 源地址是自己不要接收
	if(tmsg.Src == Address) return false;
	// 只处理本机消息或广播消息
	//if(tmsg.Dest != Address && tmsg.Dest != 0) return false;
	// 不是自己的消息，并且没有设置接收全部
	if(Mode < 2 && tmsg.Dest != Address)
	{
		// 如果不是广播或者不允许广播
		if(Mode != 1 || tmsg.Dest != 0) return false;
	}

	TS("TinyController::Valid");

#if MSG_DEBUG
	// 调试版不过滤序列号为0的重复消息
	if(tmsg.Seq != 0)
#endif
	{
		// Ack的包有可能重复，不做处理。正式响应包跟前面的Ack有相同的源地址和序列号
		if(!tmsg.Ack)
		{
			// 处理重复消息。加上来源地址，以免重复
			ushort seq = (tmsg.Src << 8) | tmsg.Seq;
			if(_Ring.Check(seq))
			{
				// 快速响应确认消息，避免对方无休止的重发
				if(!tmsg.NoAck) AckResponse(tmsg);

				msg_printf("重复消息 Src=0x%02x Seq=0x%02X Retry=%d Reply=%d Ack=%d\r\n", tmsg.Reply, tmsg.Ack, tmsg.Src, tmsg.Seq, tmsg.Retry);
				return false;
			}
			_Ring.Push(seq);
		}
	}

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

	if(tmsg.Dest == Address)
	{
		ByteArray  key(0);
		GetKey(tmsg.Src, key, Param);
		if(key.Length() > 0) Encrypt(tmsg, key);
	}
	else
	{
		//debug_printf("中转消息不解密");
	}

#if MSG_DEBUG
	// 尽量在Ack以后再输出日志，加快Ack处理速度
	ShowMessage(tmsg, false, Port);
#endif

	Total.Receive++;

	return true;
}

// 处理收到的Ack包
void TinyController::AckRequest(const TinyMessage& msg)
{
	for(int i=0; i<ArrayLength(_Queue); i++)
	{
		auto& node = _Queue[i];
		if(node.Using && node.Seq == msg.Seq)
		{
			int cost = Sys.Ms() - node.LastSend;
			if(cost < 0) cost = -cost;

			Total.Cost	+= cost;
			Total.Ack++;
			Total.Bytes	+= node.Length;

			// 发送开支作为新的随机延迟时间，这样子延迟重发就可以根据实际情况动态调整
			/*uint it = (Interval + cost) >> 1;
			// 确保小于等于超时时间的四分之一，让其有机会重发
			uint tt = Timeout >> 2;
			if(it > tt) it = tt;
			Interval = it;*/

			// 该传输口收到响应，从就绪队列中删除
			node.Using = 0;

			if(msg.Ack)
				msg_printf("收到确认 ");
			else
				msg_printf("响应确认 ");

			msg_printf("Src =0x%02x Seq=0x%02X Retry=%d Cost=%dms \r\n", msg.Src, msg.Seq, cost, msg.Retry);
			return;
		}
	}

	if(msg.Ack) msg_printf("无效确认 Src =0x%02x Seq=0x%02X Retry=%d \r\n", msg.Src, msg.Seq, msg.Retry);
}

// 向对方发出Ack包
void TinyController::AckResponse(const TinyMessage& msg)
{
	// 广播消息不要给确认
	if(msg.Dest == 0) return;

	TS("TinyController::AckResponse");

	TinyMessage msg2;
	msg2.Code	= msg.Code;
	msg2.Src	= Address;
	msg2.Dest	= msg.Src;
	msg2.Seq	= msg.Seq;
	msg2.Reply	= 1;
	msg2.Ack	= 1;
	msg2.Length	= 0;
#if MSG_DEBUG
	msg2.Retry = msg.Retry; // 说明这是匹配对方的哪一次重发
#endif

	bool rs = Controller::Send(msg2);
	//msg_printf("发送确认 Dest=0x%02x Seq=0x%02X Retry=%d \r\n", msg.Src, msg.Seq, msg.Retry);
	/*if(rs)
		msg_printf(" 成功!\r\n");
	else
		msg_printf(" 失败!\r\n");*/
}

static byte _Sequence	= 0;
// 发送消息，传输口参数为空时向所有传输口发送消息
bool TinyController::Send(Message& msg)
{
	auto& tmsg = (TinyMessage&)msg;

	// 附上自己的地址
	tmsg.Src = Address;

	// 附上序列号。响应消息保持序列号不变
	if(!tmsg.Reply) tmsg.Seq = ++_Sequence;

	ByteArray  key;
	GetKey(tmsg.Dest, key, Param);
	if(key.Length() > 0) Encrypt(tmsg, key);

#if MSG_DEBUG
	ShowMessage(tmsg, true, Port);
#endif

	return Post(tmsg, -1);
}

bool TinyController::Reply(Message& msg)
{
	auto& tmsg = (TinyMessage&)msg;

	// 回复信息，源地址变成目的地址
	if(tmsg.Dest == Address && tmsg.Src != Address) tmsg.Dest = tmsg.Src;
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
	if(msTimeout <= 0 || msg.NoAck || msg.Ack) return Controller::SendInternal(msg);

	// 针对Zigbee等不需要Ack确认的通道
	if(Timeout < 0) return Controller::SendInternal(msg);

	// 准备消息队列
	MessageNode* node = NULL;
	for(int i=0; i<ArrayLength(_Queue); i++)
	{
		if(!_Queue[i].Using)
		{
			node = &_Queue[i];
			node->Using	= 1;
			node->Seq	= 0;
			break;
		}
	}
	// 队列已满
	if(!node)
	{
		debug_printf("TinyController::Post 发送队列已满！ \r\n");
		return false;
	}

	node->Set(msg, msTimeout);

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

	int count = 0;
	for(int i=0; i<ArrayLength(_Queue); i++)
	{
		auto& node = _Queue[i];
		if(!node.Using) continue;

		// 检查时间。至少发送一次
		if(node.Next > 0)
		{
			ulong now = Sys.Ms();
			// 下一次发送时间还没到，跳过
			if(node.Next > now) continue;

			// 已过期则删除
			if(node.Expired < now)
			{
				msg_printf("消息过期 Dest=0x%02X Seq=0x%02X Times=%d\r\n", node.Data[0], node.Seq, node.Times);
				node.Using = 0;

				continue;
			}
		}

		count++;
		node.Times++;

		// 递增重试次数
		auto f = (TFlags*)&node.Data[3];
		f->Retry++;

		// 发送消息
		Port->Write(Array(node.Data, node.Length));

		// 增加发送次数统计
		Total.Send++;

		// 分组统计
		if(Total.Send >= 100)
		{
			memcpy(&Last, &Total, sizeof(Total));
			memset(&Total, 0, sizeof(Total));
		}

		// 计算下一次重发时间
		{
			ulong now	= Sys.Ms();
			node.LastSend	= now;

			// 随机延迟。随机数1~5。每次延迟递增
			//byte rnd	= (uint)now % 3;
			//node.Next	= now + (rnd + 1) * Interval;
			node.Next	= now + Interval;
		}
	}

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

	int tsend = Total.Send;
	if(tsend == lastSend && Total.Receive == lastReceive) return;
	lastSend = tsend;
	lastReceive = Total.Receive;

	uint rate = 100;
	if(Last.Send + tsend > 0)
		rate = (Last.Ack + Total.Ack) * 100 / (Last.Send + tsend);
	uint cost = 0;
	if(Last.Ack + Total.Ack > 0)
		cost = (Last.Cost + Total.Cost) / (Last.Ack + Total.Ack);
	uint speed = 0;
	if(Last.Cost + Total.Cost > 0)
		speed = (Last.Bytes + Total.Bytes) * 1000000 / (Last.Cost + Total.Cost);
	uint retry = 0;
	if(Last.Msg + Total.Msg > 0)
		retry = (Last.Send + tsend) * 100 / (Last.Msg + Total.Msg);
	msg_printf("Tiny::State 成功=%d%% 平均=%dus 速度=%d Byte/s 重发=%d.%02d 收发=%d/%d \r\n", rate, cost, speed, retry/100, retry%100, Last.Receive + Total.Receive, Last.Msg + Total.Msg);
#endif
}

/*================================ 信息节点 ================================*/
void MessageNode::Set(const TinyMessage& msg, int msTimeout)
{
	Seq			= msg.Seq;
	Times		= 0;
	LastSend	= 0;

	StartTime	= Sys.Ms();
	Expired		= StartTime + msTimeout;
	Next		= 0;

	// 注意，此时指针位于0，而内容长度为缓冲区长度
	Stream ms(Data, ArrayLength(Data));
	msg.Write(ms);
	Length		= ms.Position();
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
