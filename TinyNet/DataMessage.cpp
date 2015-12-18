#include "DataMessage.h"

// 读取数据
bool DataMessage::ReadData(Stream& ms, const DataStore& ds, uint offset, uint len)
{
	TS("DataMessage::ReadData");

	int remain	= ds.Data.Length() - offset;
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
		if(len > 0) ms.Write(ds.Data.GetBuffer(), offset, len);
		
		return true;
	}
}

// 写入数据
bool DataMessage::WriteData(Stream& ms, DataStore& ds, uint offset, Stream& ms2)
{
	TS("DataMessage::WriteData");

	// 剩余可写字节数
	uint len	= ms2.Remain();
	int remain	= ds.Data.Length() - offset;
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
		ds.Write(offset, Array(ms2.Current(), len));
		ms.WriteEncodeInt(len);
		
		return true;
	}
}
