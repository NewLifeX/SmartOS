#include "Modbus.h"

Modbus::Modbus()
{
	Address	= 0;
	Code	= 0;
	Error	= 0;
	Length	= 0;

	Crc		= 0;
	Crc2	= 0;
}

bool Modbus::Read(Stream& ms)
{
	if(ms.Remain() < 4) return false;

	byte* buf = ms.Current();
	assert_ptr(buf);
	uint p = ms.Position();

	Address	= ms.ReadByte();
	Code	= ms.ReadByte();

	if(Code & 0x80)
	{
		Code	&= 0x7F;
		Error	= 1;
	}

	Length = ms.Remain() - 2;
	ms.Read(&Data, 0, Length);

	Crc = ms.ReadUInt16();

	// 直接计算Crc16
	Crc2 = Sys.Crc16(buf, ms.Position() - p - 2);
}

void Modbus::Write(Stream& ms)
{
	uint p = ms.Position();

	ms.Write(Address);

	byte code = Code;
	if(Error) code |= 0x80;
	ms.Write(code);

	if(Length > 0) ms.Write(Data, 0, Length);

	byte* buf = ms.Current();
	byte len = ms.Position() - p;
	// 直接计算Crc16
	Crc = Crc2 = Sys.Crc16(buf - len, len);

	ms.Write(Crc);
}

void Modbus::SetError(ModbusErrors::Errors error)
{
	Code |= 0x80;
	Length = 1;
	Data[0] = error;
}
