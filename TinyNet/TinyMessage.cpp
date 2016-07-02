#include "Security\Crc.h"

#include "TinyMessage.h"
#include "Security\RC4.h"

//#define MSG_DEBUG DEBUG
#define MSG_DEBUG 0
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
	Buffer(&Dest, MinSize).Clear();

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
	//ms.Read(&Dest, 0, HeaderSize);
	Buffer bs(&Dest, HeaderSize);
	ms.Read(bs);

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
	//if(len > 0) ms.Read(Data, 0, len);
	if(len > 0)
	{
		Buffer ds(Data, len);
		ms.Read(ds);
	}

	// 读取真正的校验码
	Checksum = ms.ReadUInt16();

	// 计算Crc之前，需要清零TTL和Retry
	byte fs		= p[3];
	auto flag	= (TFlags*)&p[3];
	//flag->TTL	= 0;
	flag->Retry	= 0;
	// 连续的，可以直接计算Crc16
	Crc = Crc::Hash16(Buffer(p, HeaderSize + Length));
	// 还原数据
	p[3] = fs;

	return true;
}

void TinyMessage::Write(Stream& ms) const
{
	assert(Code, "微网指令码不能为空");
	assert(Src, "微网源地址不能为空");
	//assert(Src != Dest, "微网目的地址不能等于源地址");
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
	//ms.Write(&Dest, 0, HeaderSize);
	ms.Write(Buffer((byte*)&Dest, HeaderSize));
	if(len > 0) ms.Write(Buffer(Data, len));

	// 计算Crc之前，需要清零TTL和Retry
	byte fs		= buf[3];
	auto flag	= (TFlags*)&buf[3];
	//flag->TTL	= 0;
	flag->Retry	= 0;

	p->Checksum = p->Crc = Crc::Hash16(Buffer(buf, HeaderSize + len));

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
		msg_printf(" Data[%02d]=", len);
		ByteArray(Data, len).Show();
	}
	if(Checksum != Crc) msg_printf(" Crc Error 0x%04x [%04X]", Crc, _REV16(Crc));

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
