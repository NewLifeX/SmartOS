#ifndef __PingMessage_H__
#define __PingMessage_H__

#include "Message\MessageBase.h"
#include "Device.h"

// 心跳消息
class PingMessage : public MessageBase
{
public:
	ushort	MaxSize = 0;	// 可容纳的最大数据

	// 初始化消息，各字段为0
	PingMessage();

	// 从数据流中读取消息
	virtual bool Read(Stream& ms);
	// 把消息写入数据流中
	virtual void Write(Stream& ms);

	// 0x01 主数据
	void ReadData(Stream& ms, Array& bs);
	void WriteData(Stream& ms, byte code, const Array& bs);

	// 0x02 配置数据
	/*void ReadConfig(Stream& ms, Array& bs);
	void WriteConfig(Stream& ms, const Array& bs);*/

	// 0x03 硬件校验
	bool ReadHardCrc(Stream& ms, const Device* dv, ushort& crc);
	void WriteHardCrc(Stream& ms, ushort crc);

	// 0x04 时间
	bool ReadTime(Stream& ms, uint& seconds);
	void WriteTime(Stream& ms, uint seconds);
};

#endif
