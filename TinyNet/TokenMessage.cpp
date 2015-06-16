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
bool TokenMessage::Read(Stream& ms)
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
void TokenMessage::Write(Stream& ms)
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

	debug_printf("TokenNet::Inited 使用[%d]个传输接口 %s\r\n", _ports.Count(), _ports[0]->ToString());

	MinSize = TokenMessage::MinSize;

	_Response = NULL;
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

// 接收处理函数
bool TokenController::OnReceive(Message& msg, ITransport* port)
{
	// 如果有等待响应，则交给它
	if(msg.Reply && _Response && msg.Code == _Response->Code)
	{
		if(msg.Length > 0) _Response->SetData(msg.Data, msg.Length);
		_Response->Reply = true;

		return true;
	}

	return Controller::OnReceive(msg, port);
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

// 发送消息并接受响应，msTimeout毫秒超时时间内，如果对方没有响应，会重复发送
bool TokenController::SendAndReceive(TokenMessage& msg, int retry, int msTimeout)
{
#if DEBUG
	if(_Response) debug_printf("设计错误！正在等待Code=0x%02X的消息，完成之前不能再次调用\r\n", _Response->Code);
#endif

	if(msg.Reply) return Send(msg) != 0;

	_Response = &msg;

	bool rs = false;
	while(retry-- >= 0)
	{
		if(!Send(msg)) break;

		// 等待响应
		TimeWheel tw(0, msTimeout);
		while(!tw.Expired())
		{
			if(_Response->Reply)
			{
				rs = true;
				break;
			}
		}
		if(rs) break;
	}

	_Response = NULL;

	return rs;
}
