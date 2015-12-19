#ifndef __DataMessage_H__
#define __DataMessage_H__

#include "Stream.h"
#include "DataStore.h"
#include "Message.h"

// 数据消息
class DataMessage
{
public:
	uint	Offset;
	uint	Length;

	DataMessage(const Message& msg, Stream& dest);

	bool ReadData(const DataStore& ds);
	bool WriteData(DataStore& ds);

	bool ReadData(const Array& bs);
	bool WriteData(Array& bs);

private:
	Stream	_Src;
	Stream&	_Dest;
	//byte	_Code;
	//bool	_Reply;

	bool Write(int remain);
};

#endif
