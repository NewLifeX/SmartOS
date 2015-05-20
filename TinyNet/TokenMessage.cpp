#include "TokenMessage.h"

// 使用指定功能码初始化令牌消息
TokenMessage::TokenMessage(byte code) : Message(code)
{
	Data	= _Data;

	_Code	= 0;
	_Reply	= 0;
	_Length	= 0;
}

// 从数据流中读取消息
bool TokenMessage::Read(MemoryStream& ms)
{
	assert_ptr(this);
	if(ms.Remain() < MinSize) return false;

	byte temp = ms.Read<byte>();
	_Code	= temp & 0x7f;
	_Reply	= temp & 0x80;
	_Length	= ms.Read<byte>();
	// 占位符拷贝到实际数据
	Code	= _Code;
	Length	= _Length;
	Reply	= _Reply;

	if(ms.Remain() < Length) return false;

	assert_param(Length <= ArrayLength(_Data));
	if(Length > 0) ms.Read(Data, 0, Length);

	return true;
}

// 把消息写入到数据流中
void TokenMessage::Write(MemoryStream& ms)
{
	assert_ptr(this);
	// 实际数据拷贝到占位符
	_Code	= Code;
	_Reply	= Reply;
	_Length	= Length;

	byte tmp = _Code | (_Reply << 7);
	ms.Write(tmp);
	ms.Write(_Length);

	if(Length > 0) ms.Write(Data, 0, Length);
}

// 验证消息校验码是否有效
bool TokenMessage::Valid() const
{
	return true;
}

// 消息总长度，包括头部、负载数据和校验
uint TokenMessage::Size() const
{
	return HeaderSize + Length;
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
#if DEBUG
	assert_ptr(this);
	debug_printf("Code=0x%02x", Code);
	if(Length > 0)
	{
		assert_ptr(Data);
		debug_printf(" Data[%d]=", Length);
		Sys.ShowString(Data, Length, false);
	}
	debug_printf("\r\n");
#endif
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

	debug_printf("TokenNet::Inited 使用[%d]个传输接口\r\n", _ports.Count());

	MinSize = TokenMessage::MinSize;
}

/*TokenController::~TokenController()
{
}*/

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
	// 代码为0是非法的
	if(!msg.Code) return false;

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
	//TokenMessage& tmsg = (TokenMessage&)msg;

#if MSG_DEBUG
	ShowMessage(tmsg, true);
#endif

	return Controller::Send(msg, port);
}

/*int TokenController::Reply(Message& msg, ITransport* port)
{
	TokenMessage& tmsg = (TokenMessage&)msg;

	tmsg.Reply = 1;

	return Send(msg, port);
}*/
