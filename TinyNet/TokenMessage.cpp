#include "TokenMessage.h"

// 使用指定功能码初始化令牌消息
TokenMessage::TokenMessage(byte code) : Message(code)
{
	Data	= _Data;

	/*Token	= 0;
	_Code	= 0;
	Error	= 0;
	_Length	= 0;*/
	memset(&Token, 0, HeaderSize);

	Checksum= 0;
	Crc		= 0;
}

// 从数据流中读取消息
bool TokenMessage::Read(MemoryStream& ms)
{
	assert_ptr(this);
	if(ms.Remain() < MinSize) return false;

	ms.Read((byte*)&Token, 0, HeaderSize);
	// 占位符拷贝到实际数据
	Code = _Code;
	Length = _Length;

	if(ms.Remain() < Length + 4) return false;

	if(Length > 0) ms.Read(Data, 0, Length);

	Checksum = ms.Read<uint>();

	// 令牌消息是连续的，可以直接计算CRC
	Crc = Sys.Crc(&Token, HeaderSize + Length);

	return true;
}

// 把消息写入到数据流中
void TokenMessage::Write(MemoryStream& ms)
{
	assert_ptr(this);
	// 实际数据拷贝到占位符
	_Code = Code;
	_Length = Length;

	ms.Write((byte*)&Token, 0, HeaderSize);

	if(Length > 0) ms.Write(Data, 0, Length);

	// 令牌消息是连续的，可以直接计算CRC
	Checksum = Crc = Sys.Crc(&Token, HeaderSize + Length);

	ms.Write(Checksum);
}

void TokenMessage::ComputeCrc()
{
	MemoryStream ms(Size());

	Write(ms);

	// 扣除不计算校验码的部分
	Checksum = Crc = Sys.Crc(ms.GetBuffer(), HeaderSize + Length);
}

// 验证消息校验码是否有效
bool TokenMessage::Valid() const
{
	if(Checksum == Crc) return true;

	debug_printf("Message::Valid Crc Error %04X != Checksum: %04X \r\n", Crc, Checksum);
	return false;
}

// 消息总长度，包括头部、负载数据和校验
uint TokenMessage::Size() const
{
	return HeaderSize + Length + 4;
}

// 设置错误信息字符串
void TokenMessage::SetError(byte errorCode, string error, int errLength)
{
	Length = 1 + errLength;
	Data[0] = errorCode;
	if(errLength > 0)
	{
		assert_ptr(error);
		memcpy(Data + 1, error, errLength);
	}
}

void TokenMessage::Show() const
{
	assert_ptr(this);
	debug_printf("Token=0x%08x Code=0x%02x", Token, Code);
	if(Length > 0)
	{
		assert_ptr(Data);
		debug_printf("Data[%d]=", Length);
		Sys.ShowString(Data, Length, false);
	}
	if(!Valid()) debug_printf(" Crc Error 0x%04x [%04X]", Crc, __REV(Crc));
	debug_printf("\r\n");
}

TokenController::TokenController(ITransport* port) : Controller(port)
{
	Init();
}

TokenController::TokenController(ITransport* ports[], int count) : Controller(ports, count)
{
	Init();
}

void TokenController::Init()
{
	Token	= 0;

	MinSize = TokenMessage::MinSize;
}

TokenController::~TokenController()
{
}

// 创建消息
Message* TokenController::Create() const
{
	return new TokenMessage();
}

// 发送消息，传输口参数为空时向所有传输口发送消息
bool TokenController::Send(byte code, byte* buf, uint len)
{
	TokenMessage msg;
	msg.Code = code;
	msg.SetData(buf, len);

	return Send(msg);
}

// 发送消息
/*bool TokenController::Send(TokenMessage& msg, int expire)
{
	MemoryStream ms(msg.Size());
	msg.Write(ms);

	while(true)
	{
		//_port->Write(ms.GetBuffer(), ms.Length);

		if(expire <= 0) break;

		// 等待响应
		Sys.Sleep(1);
	}

	return true;
}*/

// 收到消息校验后调用该函数。返回值决定消息是否有效，无效消息不交给处理器处理
bool TokenController::Valid(Message& msg, ITransport* port)
{
	TokenMessage& tmsg = (TokenMessage&)msg;

	// 代码为0是非法的
	if(!msg.Code) return false;
	// 只处理本机消息或广播消息。快速处理，高效。
	if(tmsg.Token != Token && tmsg.Token != 0) return false;

#if MSG_DEBUG
	/*msg_printf("TokenController::Dispatch ");
	// 输出整条信息
	Sys.ShowHex(buf, ms.Length, '-');
	msg_printf("\r\n");*/
#endif

	return true;
}

// 发送消息，传输口参数为空时向所有传输口发送消息
int TokenController::Send(Message& msg, ITransport* port)
{
	TokenMessage& tmsg = (TokenMessage&)msg;

	// 附上自己的地址
	tmsg.Token = Token;

#if MSG_DEBUG
	// 计算校验
	msg.ComputeCrc();

	ShowMessage(tmsg, true);
#endif

	return Controller::Send(msg, port);
}

int TokenController::Reply(Message& msg, ITransport* port)
{
	TokenMessage& tmsg = (TokenMessage&)msg;

	tmsg.Reply = 1;

	return Send(msg, port);
}
