#ifndef __DataMessage_H__
#define __DataMessage_H__

#include "Stream.h"

// 数据消息
class DataMessage
{
public:
	static bool ReadData(Stream& ms, const Array& bs, uint offset, uint len);
	static bool WriteData(Stream& ms, Array& bs, uint offset, Stream& ds);
};

#endif
