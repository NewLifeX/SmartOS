#include "DataMessage.h"

// 读取数据
bool ReadData(Stream& ms, const Array& bs, uint offset, uint len)
{
	TS("DataMessage::ReadData");

	int remain	= bs.Length() - offset;
	if(remain < 0)
	{
		ms.Write((byte)2);
		ms.WriteEncodeInt(offset);
		ms.WriteEncodeInt(len);
		
		return false;
	}
	else
	{
		ms.WriteEncodeInt(offset);
		if(len > remain) len = remain;
		if(len > 0) ms.Write(bs.GetBuffer(), offset, len);
		
		return true;
	}
}

// 写入数据
bool WriteData(Stream& ms, Array& bs, uint offset, Stream& ds)
{
	TS("DataMessage::WriteData");

	// 剩余可写字节数
	uint len	= ds.Remain();
	int remain	= bs.Length() - offset;
	if(remain < 0)
	{
		ms.Write((byte)2);
		ms.WriteEncodeInt(offset);
		ms.WriteEncodeInt(len);
		
		return false;
	}
	else
	{
		ms.WriteEncodeInt(offset);

		if(len > remain) len = remain;
		bs.Copy(ds.Current(), len);
		ms.WriteEncodeInt(len);
		
		return true;
	}
}
