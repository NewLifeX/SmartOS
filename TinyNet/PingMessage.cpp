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
void PingMessage::Write(Stream& ms)
{
}

// 0x01 主数据
void PingMessage::ReadData(Stream& ms, Array& bs)
{
	TS("PingMessage::ReadData");

	byte offset	= ms.ReadByte();
	byte len	= ms.ReadByte();
	//debug_printf("设备 0x%02X 同步数据（%d, %d）到网关缓存 \r\n", dv->Address, offset, len);
	debug_printf(" 同步数据（%d, %d）到网关缓存 \r\n", offset, len);

	int remain	= bs.Capacity() - offset;
	int len2	= len;
	if(len2 > remain) len2 = remain;
	// 保存一份到缓冲区
	if(len2 > 0)
	{
		bs.Copy(ms.ReadBytes(len), len2, offset);
	}
}

void PingMessage::WriteData(Stream& ms, byte code, const Array& bs)
{
	TS("PingMessage::WriteData");

	byte len = bs.Length() - 1;
	//if(ms.Position() + 3 + len > MaxSize) return;
	byte remain	= MaxSize - ms.Position() - 3;
	if(len > remain) len = remain;

	ms.Write(code);	// 子功能码
	ms.Write((byte)0x01);	// 起始地址

	ms.Write(len);	// 长度
	ms.Write(Array((byte*)bs.GetBuffer() + 1, len));
}


/*// 0x02 配置数据
void PingMessage::ReadConfig(Stream& ms, Array& bs)
{
	byte offset = ms.ReadByte();
	byte len	= ms.ReadByte();
	ms.SetPosition(ms.Position() + len);
}

void PingMessage::WriteConfig(Stream& ms, const Array& bs)
{
	TS("PingMessage::WriteConfig");

	byte len = bs.Length() - 1;
	if(len > 0x10) len = 0x10;
	if(ms.Position() + 3 + len > MaxSize) return;

	ms.Write((byte)0x02);	// 子功能码
	ms.Write((byte)0x01);	// 起始地址

	ms.Write(len);	// 长度
	ms.Write(Array((byte*)bs.GetBuffer() + 1, len));
}*/


// 0x03 硬件校验
bool PingMessage::ReadHardCrc(Stream& ms, const Device* dv, ushort& crc)
{
	crc  = ms.ReadUInt16();
	ushort crc1 = Crc::Hash16(dv->HardID);
	// 下一行仅调试使用
	//debug_printf("设备硬件Crc: %08X, 本地Crc：%08X \r\n", crc, crc1);
	if(crc != crc1)
	{
		debug_printf("设备硬件Crc: %04X, 本地Crc：%04X \r\n", crc, crc1);
		debug_printf("设备硬件ID: ");
		dv->HardID.Show(true);

		return false;
	}

	return true;
}

void PingMessage::WriteHardCrc(Stream& ms, ushort crc)
{
	TS("PingMessage::WriteHardCrc");

	if(ms.Position() + 3 > MaxSize) return;

	ms.Write((byte)0x03);	// 子功能码
	ms.Write(crc);		//硬件CRC
}


// 0x04 时间
bool PingMessage::ReadTime(Stream& ms, uint& seconds)
{
	if(ms.Remain() < 4) return false;

	seconds	= ms.ReadUInt32();

	return true;
}

void PingMessage::WriteTime(Stream& ms, uint seconds)
{
	TS("PingMessage::WriteTime");

	if(ms.Position() + 5 > MaxSize) return;

	ms.Write((byte)0x04);	// 子功能码
	ms.Write(seconds);		//硬件CRC
}
