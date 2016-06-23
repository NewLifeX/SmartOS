#include "TokenMessage.h"

#include "Net\Socket.h"
#include "Security\RC4.h"
#include "Security\Crc.h"

#define MSG_DEBUG DEBUG
//#define MSG_DEBUG 0

/******************************** TokenMessage ********************************/

// 使用指定功能码初始化令牌消息
TokenMessage::TokenMessage(byte code) : Message(code)
{
	Data	= _Data;

	//OneWay	= false;
	Seq		= 0;
}

// 从数据流中读取消息
bool TokenMessage::Read(Stream& ms)
{
	TS("TokenMessage::Read");

	assert_ptr(this);
	if(ms.Remain() < MinSize) return false;

	byte temp = ms.ReadByte();
	Code	= temp & 0x0F;
	Reply	= temp >> 7;
	if(!Reply)
		OneWay	= (temp >> 6) & 0x01;
	else
		Error	= (temp >> 6) & 0x01;
	Seq		= ms.ReadByte();
	ushort len	= ms.ReadEncodeInt();
	Length	= len;

	if(ms.Remain() < len) return false;

	// 避免错误指令超长，导致溢出 data后面有crc
	if(Data == _Data && (len + 2) > ArrayLength(_Data))
	{
		debug_printf("错误指令，长度 %d 大于消息数据缓冲区长度 %d \r\n", len, ArrayLength(_Data));
		//assert_param(false);
		return false;
	}
	if(len > 0)
	{
		Buffer bs(Data, len+2);	// DATA + CRC 位置
		ms.Read(bs);
	}

	return true;
}

// 把消息写入到数据流中
void TokenMessage::Write(Stream& ms) const
{
	TS("TokenMessage::Write");

	assert_ptr(this);	
	byte tmp = Code | (Reply << 7);
	if((!Reply && OneWay) || (Reply && Error)) tmp |= (1 << 6);
	ms.Write(tmp);
	ms.Write(Seq);
	ms.WriteArray(Buffer(Data, Length));
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

uint TokenMessage::MaxDataSize() const
{
	return Data == _Data ? ArrayLength(_Data) : Length;
}

// 创建当前消息对应的响应消息。设置序列号、标识位
TokenMessage TokenMessage::CreateReply() const
{
	TokenMessage msg;
	msg.Code	= Code;
	msg.Reply	= true;
	msg.Seq		= Seq;
	msg.State	= State;

	return msg;
}

void TokenMessage::Show() const
{
#if MSG_DEBUG
	TS("TokenMessage::Show");

	assert_ptr(this);

	byte code = Code;
	if(Reply) code |= 0x80;
	if((!Reply && OneWay) || (Reply && Error)) code |= (1 << 6);

	debug_printf("%02X", code);
	if(Reply)
	{
		if(Error)
			debug_printf("$");
		else
			debug_printf("#");
	}
	else
	{
		if(OneWay)
			debug_printf("~");
		else
			debug_printf(" ");
	}
	debug_printf(" Seq=%02X", Seq);

	ushort len	= Length;
	if(len > 0)
	{
		assert_ptr(Data);
		debug_printf(" Data[%d]=",len);
		// 大于32字节时，反正都要换行显示，干脆一开始就换行，让它对齐
		//if(len > 32) debug_printf("\r\n");
		if(len > 32) len	= 32;
		ByteArray(Data, len).Show();
	}
	debug_printf("\r\n");
#endif
}
