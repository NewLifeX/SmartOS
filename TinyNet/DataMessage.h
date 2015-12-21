#ifndef __DataMessage_H__
#define __DataMessage_H__

#include "Stream.h"
#include "Message\DataStore.h"
#include "Message\Message.h"

// 数据消息
class DataMessage
{
public:
	uint	Offset;
	uint	Length;

	DataMessage(const Message& msg);
	DataMessage(const Message& msg, Stream& dest);

	bool ReadData(const DataStore& ds);
	bool WriteData(DataStore& ds, bool withData);

	bool ReadData(const Array& bs);
	bool WriteData(Array bs, bool withData);

private:
	Stream	_Src;
	Stream*	_Dest;

	bool Write(int remain);
};

#endif
