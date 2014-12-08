#include "TokenMessage.h"

TokenMessage::TokenMessage()
{
	Token	= 0;
	Code	= 0;
	Error	= 0;
	Length	= 0;

	Crc		= 0;
	Crc2	= 0;
}

bool TokenMessage::Read(MemoryStream& ms)
{
	if(ms.Remain() < 10) return false;

	ms.Read((byte*)&Token, 0, 6);

	if(ms.Remain() - 4 < Length) return false;

	ms.Read(Data, 0, Length);

	Crc = ms.Read<uint>();

	// 令牌消息是连续的，可以直接计算CRC
	Crc2 = Sys.Crc(&Token, Size() - 4, 0);

	return true;
}

void TokenMessage::Write(MemoryStream& ms)
{
	uint p = ms.Position();

	ms.Write((byte*)&Token, 0, 6);

	if(Length > 0) ms.Write(Data, 0, Length);

	Sys.ShowHex(ms.GetBuffer(), ms.Length, '-');
	debug_printf("\r\n");
	// 令牌消息是连续的，可以直接计算CRC
	Crc = Crc2 = Sys.Crc(&Token, Size() - 4);
	debug_printf("Crc=0x%08X\r\n", Crc);

	ms.Write(Crc);
}

uint TokenMessage::Size() const
{
	return 4 + 1 + 1 + Length + 4;
}

// 设置数据。
void TokenMessage::SetData(byte* buf, uint len)
{
	assert_param(len <= ArrayLength(Data));

	Length = len;
	if(len > 0)
	{
		assert_ptr(buf);
		memcpy(Data, buf, len);
	}
}

void TokenMessage::SetError(byte error)
{
	Error = 1;
	Length = 1;
	Data[0] = error;
}

TokenController::TokenController(ITransport* port)
{
	assert_ptr(port);
	_port = port;

	debug_printf("\r\nTokenController::Init 传输口：%s\r\n", port->ToString());

	// 注册收到数据事件
	port->Register(Dispatch, this);
}

TokenController::~TokenController()
{
	_port->Register(NULL, NULL);

	delete _port;
	_port = NULL;
}

// 发送消息，传输口参数为空时向所有传输口发送消息
bool TokenController::Send(byte code, byte* buf, uint len)
{
	TokenMessage msg;
	msg.Code = code;
	msg.Length = len;
	memcpy(msg.Data, buf, len);

	return Send(msg);
}

// 发送消息，expire毫秒超时时间内，如果对方没有响应，会重复发送
bool TokenController::Send(TokenMessage& msg, int expire)
{
	MemoryStream ms(msg.Size());
	msg.Write(ms);

	while(true)
	{
		_port->Write(ms.GetBuffer(), ms.Length);

		if(expire <= 0) break;

		// 等待响应
		Sys.Sleep(1);
	}

	return true;
}

uint TokenController::Dispatch(ITransport* transport, byte* buf, uint len, void* param)
{
	assert_ptr(buf);
	assert_ptr(param);

	TokenController* control = (TokenController*)param;

	// 这里使用数据流，可能多个消息粘包在一起
	// 注意，此时指针位于0，而内容长度为缓冲区长度
	MemoryStream ms(buf, len);
	while(ms.Remain() >= 10)
	{
		// 如果不是有效数据包，则直接退出，避免产生死循环。当然，也可以逐字节移动测试，不过那样性能太差
		if(!control->Dispatch(ms, transport)) break;
	}

	return 0;
}

bool TokenController::Dispatch(MemoryStream& ms, ITransport* port)
{
	byte* buf = ms.Current();
	// 代码为0是非法的
	if(!buf[4]) return 0;
	// 只处理本机消息或广播消息。快速处理，高效。
	uint addr = *(uint*)buf;
	if(addr != Address && addr != 0) return false;

#if MSG_DEBUG
	/*msg_printf("TokenController::Dispatch ");
	// 输出整条信息
	Sys.ShowHex(buf, ms.Length, '-');
	msg_printf("\r\n");*/
#endif

	TokenMessage msg;
	if(!msg.Read(ms)) return false;

	// 校验
	if(msg.Crc != msg.Crc2)
	{
		msg.SetError(1);
		Send(msg);

		return true;
	}

#if MSG_DEBUG
	//ShowMessage(msg, false, port);
#endif

	// 外部公共消息事件
	if(Received)
	{
		if(!Received(msg, Param)) return true;
	}

	// 选择处理器来处理消息
	for(int i=0; i<_HandlerCount; i++)
	{
		HandlerLookup* lookup = _Handlers[i];
		if(lookup && lookup->Code == msg.Code)
		{
			// 返回值决定是普通回复还是错误回复
			bool rs = lookup->Handler(msg, lookup->Param);

			if(rs) Send(msg);

			break;
		}
	}

	return true;
}
