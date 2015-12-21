#include "DataMessage.h"

DataMessage::DataMessage(const Message& msg, Stream* dest) : _Src(msg.ToStream())
{
	_Dest	= dest;

	Offset	= _Src.ReadEncodeInt();
	Length	= 0;

	// 读取请求、写入响应、错误响应 等包含偏移和长度
	byte code	= msg.Code & 0x0F;
	if(code == 0x05 && !msg.Reply || code == 0x06 && msg.Reply || msg.Error)
		Length	= _Src.ReadEncodeInt();
}

// 读取数据
bool DataMessage::ReadData(const DataStore& ds)
{
	return ReadData(ds.Data);
}

// 读取数据
bool DataMessage::ReadData(const Array& bs)
{
	if(!_Dest) return false;

	TS("DataMessage::ReadData");

	auto& ms	= *_Dest;
	int remain	= bs.Length() - Offset;
	if(remain < 0)
	{
		ms.Write((byte)2);
		ms.WriteEncodeInt(Offset);
		ms.WriteEncodeInt(Length);

		return false;
	}
	else
	{
		ms.WriteEncodeInt(Offset);
		if(Length > remain) Length = remain;
		if(Length > 0) ms.Write(bs.GetBuffer(), Offset, Length);

		return true;
	}
}

// 写入数据
bool DataMessage::WriteData(DataStore& ds, bool withData)
{
	TS("DataMessage::WriteData");

	Length	= _Src.Remain();
	if(_Dest && !Write(ds.Data.Length() - Offset)) return false;

	Array dat(_Src.Current(), Length);
	ds.Write(Offset, dat);

	// 如果携带数据，则把这一段数据附加到后面
	if(_Dest && withData) _Dest->Write(ds.Data.GetBuffer(), Offset, Length);

	return true;
}

// 写入数据
bool DataMessage::WriteData(Array bs, bool withData)
{
	TS("DataMessage::WriteData");

	// 剩余可写字节数
	Length	= _Src.Remain();
	if(_Dest && !Write(bs.Length() - Offset)) return false;

	Array dat(_Src.Current(), Length);
	bs.Copy(dat, Offset);

	// 如果携带数据，则把这一段数据附加到后面
	if(_Dest && withData) _Dest->Write(bs.GetBuffer(), Offset, Length);

	return true;
}

// 写入数据
bool DataMessage::Write(int remain)
{
	auto& ms	= *_Dest;
	// 剩余可写字节数
	if(remain < 0)
	{
		ms.Write((byte)2);
		ms.WriteEncodeInt(Offset);
		ms.WriteEncodeInt(Length);

		return false;
	}
	else
	{
		ms.WriteEncodeInt(Offset);

		if(Length > remain) Length = remain;
		ms.WriteEncodeInt(Length);

		return true;
	}
}
