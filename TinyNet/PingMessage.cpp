#include "PingMessage.h"
#include "TinyServer.h"

#include "Security\Crc.h"

// 初始化消息，各字段为0
PingMessage::PingMessage()
{
}

// 从数据流中读取消息
bool PingMessage::Read(Stream& ms)
{
	return true;
}

// 把消息写入数据流中
void PingMessage::Write(Stream& ms) const
{
}

// 0x01 主数据
void PingMessage::ReadData(Stream& ms, Buffer& bs) const
{
	TS("PingMessage::ReadData");

	byte offset	= ms.ReadByte();
	byte len	= ms.ReadByte();

	int remain	= bs.Length() - offset;
	int len2	= len;
	if(len2 > remain) len2 = remain;
	// 保存一份到缓冲区
	if(len2 > 0)
	{
		bs.Copy(offset, ms.ReadBytes(len), len2);
	}
}

// 写入数据。同时写入头部大小，否则网关不知道数据区大小和配置区大小
void PingMessage::WriteData(Stream& ms, byte code, const Buffer& bs) const
{
	TS("PingMessage::WriteData");

	int remain	= MaxSize - ms.Position() - 3;
	if(remain <= 0) return;

	byte len	= bs.Length();
	if(len > remain) len = remain;

	ms.Write(code);	// 子功能码
	ms.Write((byte)0x00);	// 起始地址

	ms.Write(len);	// 长度
	ms.Write(bs.Sub(len));
}

// 0x03 硬件校验
bool PingMessage::ReadHardCrc(Stream& ms, const Device& dv, ushort& crc) const
{
	crc  = ms.ReadUInt16();
	ushort crc1 = Crc::Hash16(dv.HardID);
	if(crc != crc1)
	{
		debug_printf("设备硬件Crc: %04X, 本地Crc：%04X \r\n", crc, crc1);
		debug_printf("设备硬件ID: ");
		//ByteArray(dv->HardID, ArrayLength(dv->HardID)).Show(true);
		dv.HardID.Show();

		return false;
	}

	return true;
}

void PingMessage::WriteHardCrc(Stream& ms, ushort crc) const
{
	TS("PingMessage::WriteHardCrc");

	if(ms.Position() + 3 > MaxSize) return;

	ms.Write((byte)0x03);	// 子功能码
	ms.Write(crc);		//硬件CRC
}


// 0x04 时间
bool PingMessage::ReadTime(Stream& ms, uint& seconds) const
{
	if(ms.Remain() < 4) return false;

	seconds	= ms.ReadUInt32();

	return true;
}

void PingMessage::WriteTime(Stream& ms, uint seconds) const
{
	TS("PingMessage::WriteTime");

	if(ms.Position() + 5 > MaxSize) return;

	ms.Write((byte)0x04);	// 子功能码
	ms.Write(seconds);
}
