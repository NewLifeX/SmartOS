#include "TokenMessage.h"

TokenMessage::TokenMessage()
{
	Code	= 0;
	Error	= 0;
	Length	= 0;

	Crc		= 0;
	Crc2	= 0;
}

bool TokenMessage::Read(MemoryStream& ms)
{
	if(ms.Remain() < 4) return false;

	//byte* buf = ms.Current();
	//assert_ptr(buf);
	//uint p = ms.Position();

	*(byte*)&Code = ms.ReadByte();

	/*if(Code & 0x80)
	{
		Code	&= 0x7F;
		Error	= 1;
	}*/

	//Length = ms.Remain() - 2;
	Length = ms.ReadByte();
	if(ms.Remain() - 2 < Length) return false;

	ms.Read(&Data, 0, Length);

	Crc = ms.Read<ushort>();

	// 直接计算Crc16
	//Crc2 = Sys.Crc16(buf, ms.Position() - p - 2);
	// 令牌消息是连续的，可以直接计算CRC
	Crc2 = Sys.Crc16(&Code, 1 + 1 + Length);
}

void TokenMessage::Write(MemoryStream& ms)
{
	uint p = ms.Position();

	//byte code = Code;
	//if(Error) code |= 0x80;
	ms.Write(*(byte*)&Code);

	ms.Write(Length);
	if(Length > 0) ms.Write(Data, 0, Length);

	//byte* buf = ms.Current();
	//byte len = ms.Position() - p;
	// 直接计算Crc16
	//Crc = Crc2 = Sys.Crc16(buf - len, len);
	// 令牌消息是连续的，可以直接计算CRC
	Crc = Crc2 = Sys.Crc16(&Code, 1 + 1 + Length);

	ms.Write(Crc);
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
	//Code |= 0x80;
	Error = 1;
	Length = 1;
	Data[0] = error;
}

TokenController::TokenController(ITransport* port)
{
	_port = port;
}

TokenController::~TokenController()
{
	delete _port;
	_port = NULL;
}

// 发送消息，传输口参数为空时向所有传输口发送消息
bool TokenController::Send(byte code, byte* buf, uint len)
{
	TokenMessage msg;
	msg.Code = code;
	
	return Send(msg, -1);
}

// 发送消息，expire毫秒超时时间内，如果对方没有响应，会重复发送
bool TokenController::Send(TokenMessage& msg, int expire)
{
}
