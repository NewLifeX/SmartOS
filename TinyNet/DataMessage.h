#ifndef __DataMessage_H__
#define __DataMessage_H__

#include "Stream.h"
#include "DataStore.h"

// 数据消息
class DataMessage
{
public:
	static bool ReadData(Stream& ms, const DataStore& ds, uint offset, uint len);
	static bool WriteData(Stream& ms, DataStore& ds, uint offset, Stream& ms2);
};

#endif
